"""Send durable game results directly to Google. This does not host a server."""
import argparse
import json
import logging
from pathlib import Path
import re
import time
import urllib.request
from urllib.parse import urlsplit

ROOT=Path(__file__).resolve().parent.parent
CONFIG=ROOT/"cloud/private/connection.json"
MAX_EVENT=16384


def connection(path=CONFIG):
    data=json.loads(Path(path).read_text(encoding="utf-8-sig"))
    url=data.get("endpoint", "")
    parsed=urlsplit(url)
    if parsed.scheme!="https" or parsed.netloc!="script.google.com" or not re.fullmatch(r"/macros/s/[A-Za-z0-9_-]+/exec",parsed.path) or parsed.query or parsed.fragment:
        raise ValueError("A deployed Google Apps Script /exec URL is required")
    key=data.get("uploadKey", "")
    if not isinstance(key,str) or not re.fullmatch(r"[A-Za-z0-9_-]{32,256}",key):
        raise ValueError("Invalid upload key")
    return {"endpoint":url,"uploadKey":key}


class GoogleRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,request,fp,code,message,headers,newurl):
        target=urlsplit(newurl)
        if target.scheme!="https" or target.hostname not in {"script.google.com","script.googleusercontent.com"} or target.username or target.password:
            raise ValueError("Unexpected cloud redirect")
        return super().redirect_request(request,fp,code,message,headers,newurl)


def send(settings,record):
    # The key goes in the POST body, never into a URL or a public web asset.
    body=json.dumps({"uploadKey":settings["uploadKey"],"record":record},ensure_ascii=False).encode("utf-8")
    request=urllib.request.Request(settings["endpoint"],data=body,headers={"Content-Type":"application/json"})
    with urllib.request.build_opener(GoogleRedirects()).open(request,timeout=20) as response:
        raw=response.read(65537)
    if len(raw)>65536:
        raise ValueError("Cloud response is too large")
    try:
        result=json.loads(raw)
    except (ValueError,UnicodeError):
        raise ValueError("Cloud returned a page instead of JSON; check web app deployment access") from None
    if not isinstance(result,dict) or result.get("ok") is not True or result.get("eventId")!=record.get("eventId"):
        # Do not log a third-party response body (or any possible credentials).
        raise ValueError("Cloud did not acknowledge this event; result remains queued")
    return result


def process_once(outbox,settings,sender=send):
    outbox=Path(outbox).resolve()
    outbox.mkdir(parents=True,exist_ok=True)
    sent=0
    for path in sorted(outbox.glob("*.json"))[:100]:
        if path.is_symlink():
            continue
        try:
            if path.stat().st_size>MAX_EVENT:
                raise ValueError("Game result is too large")
            record=json.loads(path.read_text(encoding="utf-8"))
            if not isinstance(record,dict) or not isinstance(record.get("eventId"),str):
                raise ValueError("Invalid game result")
            result=sender(settings,record)
            if not isinstance(result,dict) or result.get("ok") is not True or result.get("eventId")!=record["eventId"]:
                raise ValueError("Cloud did not acknowledge this event")
            # Keep local receipts outside the public DriveX folder. Writing the
            # receipt first makes a crash/retry safe through cloud event IDs.
            receipts=outbox.parent/"cloud-sent"
            receipts.mkdir(exist_ok=True)
            (receipts/path.name).write_text(json.dumps(result,ensure_ascii=False),encoding="utf-8")
            path.unlink()
            sent+=1
        except Exception as error:
            logging.warning("Result retained (%s): %s",path.name,type(error).__name__)
    return sent


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--once",action="store_true")
    parser.add_argument("--config",type=Path,default=CONFIG)
    parser.add_argument("--outbox",type=Path,default=ROOT/"server/outbox")
    args=parser.parse_args()
    logging.basicConfig(level=logging.INFO,format="%(asctime)s %(levelname)s %(message)s")
    try:
        settings=connection(args.config)
    except (OSError,ValueError):
        logging.error("Cloud connection is not configured. Run configure_cloud.bat; queued game results are preserved.")
        return 2
    logging.info("Cloud score sender started; no listening port is opened.")
    while True:
        count=process_once(args.outbox,settings)
        if count:
            logging.info("Cloud acknowledged %d result(s).",count)
        if args.once:
            return 0
        time.sleep(5)


if __name__=="__main__":
    raise SystemExit(main())
