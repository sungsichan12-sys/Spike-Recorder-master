import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import sync_scores as sync


class SenderTests(unittest.TestCase):
    def setUp(self):
        parent=Path(__file__).resolve().parent/"test-artifacts"
        parent.mkdir(exist_ok=True)
        self.fixture=tempfile.TemporaryDirectory(prefix="sender-",dir=parent)
        self.root=Path(self.fixture.name).resolve()
        self.assertEqual(self.root.parent,parent.resolve())
        self.addCleanup(self.fixture.cleanup)
        self.outbox=self.root/"outbox"
        self.outbox.mkdir()
        self.settings={"endpoint":"https://script.google.com/macros/s/TEST/exec","uploadKey":"a"*64}

    def write(self,name="round-1",**fields):
        path=self.outbox/(name+".json")
        path.write_text(json.dumps({"eventId":name,"playerName":"민수",**fields}),encoding="utf-8")
        return path

    def test_delete_only_after_matching_cloud_acknowledgement_and_receipt(self):
        path=self.write()
        self.assertEqual(sync.process_once(self.outbox,self.settings,lambda s,r:{"ok":True,"eventId":r["eventId"]}),1)
        self.assertFalse(path.exists())
        self.assertEqual(json.loads((self.root/"cloud-sent/round-1.json").read_text())["eventId"],"round-1")

    def test_network_failure_or_wrong_ack_keeps_original_result(self):
        path=self.write()
        for response in [{"ok":False},{"ok":True,"eventId":"someone-else"}]:
            self.assertEqual(sync.process_once(self.outbox,self.settings,lambda s,r:response),0)
            self.assertTrue(path.exists())
        def offline(settings,record):
            raise ConnectionError("offline")
        self.assertEqual(sync.process_once(self.outbox,self.settings,offline),0)
        self.assertTrue(path.exists())

    def test_incomplete_events_are_ignored_and_malformed_does_not_block_valid(self):
        (self.outbox/"incomplete.json.tmp").write_text("{")
        (self.outbox/"bad.json").write_text("not json")
        self.write()
        self.assertEqual(sync.process_once(self.outbox,self.settings,lambda s,r:{"ok":True,"eventId":r["eventId"]}),1)
        self.assertTrue((self.outbox/"incomplete.json.tmp").exists())
        self.assertTrue((self.outbox/"bad.json").exists())

    def test_credentials_require_google_deployment_and_remain_in_post_body(self):
        path=self.root/"connection.json"
        path.write_text(json.dumps(self.settings))
        self.assertEqual(sync.connection(path),self.settings)
        for url in ["http://localhost",self.settings["endpoint"]+"?secret=x","https://evil.test/macros/s/x/exec"]:
            path.write_text(json.dumps({**self.settings,"endpoint":url}))
            with self.assertRaises(ValueError):
                sync.connection(path)
        captured={}
        class Response:
            def __enter__(self): return self
            def __exit__(self,*args): pass
            def read(self,size): return b'{"ok":true,"eventId":"round-1"}'
        class Opener:
            def open(self,request,timeout):
                captured["request"]=request
                return Response()
        with patch.object(sync.urllib.request,"build_opener",return_value=Opener()):
            sync.send(self.settings,{"eventId":"round-1","playerName":"민수"})
        request=captured["request"]
        self.assertNotIn(self.settings["uploadKey"],request.full_url)
        self.assertEqual(json.loads(request.data)["record"]["playerName"],"민수")


if __name__=="__main__":
    unittest.main()
