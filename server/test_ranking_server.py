import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen
from ranking_server import ROOT, RankingServer, Store


class RankingTests(unittest.TestCase):
    def setUp(self):
        fixture_root = (ROOT / "server/test-artifacts").resolve()
        fixture_root.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="lottol-ranking-", dir=fixture_root)
        self.root = Path(self.temp.name).resolve()
        if self.root.parent != fixture_root:
            raise RuntimeError("Test fixture escaped its dedicated workspace")
        for game, name in (("dino", "Signal Dino"), ("night-pulse", "Night Pulse 3D")):
            folder = self.root / "Spike-Recorder-master/game" / game
            folder.mkdir(parents=True)
            (folder / "game.ini").write_text(f"id={game}\ntitle={name}\ntype=plugin\n", encoding="utf-8")
        for name in ("index.html", "arena-client.js", "arena-page.js"):
            shutil.copyfile(ROOT / name, self.root / name)
        self.store = Store(self.root)
        self.server = RankingServer(("127.0.0.1", 0), self.store, ["https://arena.example"])
        self.thread = threading.Thread(target=self.server.serve_forever, kwargs={"poll_interval": .01}, daemon=True)
        self.thread.start()
        self.base = f"http://127.0.0.1:{self.server.server_port}"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.temp.cleanup()

    def request(self, path, data=None, headers=None, method=None):
        body = json.dumps(data).encode("utf-8") if data is not None else None
        headers = {"Content-Type": "application/json", **(headers or {})}
        request = Request(self.base + path, data=body, headers=headers, method=method)
        try:
            response = urlopen(request, timeout=5)
        except HTTPError as error:
            response = error
        with response:
            contents = response.read()
            value = json.loads(contents) if response.headers.get("Content-Type", "").startswith("application/json") else contents
            return response.status, value, response.headers

    def score(self, event="test-1", **values):
        return {"eventId": event, "playerName": "학생 1", "gameId": "dino", "score": 100, **values}

    def test_health_and_all_page_assets_are_served(self):
        self.assertEqual(self.request("/api/health")[1]["app"], "lottol-ranking")
        for path in ("/", "/index.html", "/arena-client.js", "/arena-page.js"):
            status, body, _ = self.request(path)
            self.assertEqual(status, 200)
            self.assertTrue(body)

    def test_database_and_source_files_are_not_public(self):
        for path in ("/server/data/rankings.sqlite3", "/server/ranking_server.py", "/../server/data/rankings.sqlite3"):
            self.assertEqual(self.request(path)[0], 404)

    def test_game_list_matches_installed_manifests(self):
        self.assertEqual({g["id"] for g in self.request("/api/games")[1]["games"]}, {"dino", "night-pulse"})

    def test_unicode_score_submission_can_be_read(self):
        self.assertEqual(self.request("/api/scores", self.score())[0], 201)
        row = self.request("/api/scores")[1]["scores"][0]
        self.assertEqual((row["playerName"], row["score"], row["source"]), ("학생 1", 100, "manual"))

    def test_duplicate_retry_does_not_create_a_second_record(self):
        self.request("/api/scores", self.score())
        status, result, _ = self.request("/api/scores", self.score())
        self.assertEqual(status, 200)
        self.assertTrue(result["duplicate"])
        with self.store.db() as con:
            self.assertEqual(con.execute("SELECT COUNT(*) FROM scores").fetchone()[0], 1)
        self.assertEqual(self.request("/api/scores", self.score(score=101))[0], 400)

    def test_scores_names_and_unknown_games_are_validated(self):
        for value in (-1, 1.2, "100", True, 1000000001, float("nan")):
            self.assertEqual(self.request("/api/scores", self.score(score=value))[0], 400)
        for name in ("", "  ", "a" * 33, "a\nb"):
            self.assertEqual(self.request("/api/scores", self.score(playerName=name))[0], 400)
        self.assertEqual(self.request("/api/scores", self.score(gameId="unknown"))[0], 400)

    def test_server_controls_timestamp(self):
        result = self.request("/api/scores", self.score(createdAt="2999-01-01T00:00:00Z"))[1]
        self.assertFalse(result["record"]["createdAt"].startswith("2999"))

    def test_offline_program_result_preserves_play_time(self):
        row, _ = self.store.save(self.score("offline", recordedAtMs=1577836800000), source="program")
        self.assertTrue(row["createdAt"].startswith("2020-01-01"))
        self.assertFalse(self.store.scores(period="week"))

    def test_only_personal_best_is_ranked_and_game_filter_works(self):
        for event, name, game, score in (("a", "PLAYER", "dino", 100), ("b", "player", "dino", 20), ("c", "PLAYER", "night-pulse", 300)):
            self.request("/api/scores", self.score(event, playerName=name, gameId=game, score=score))
        rows = self.request("/api/scores")[1]["scores"]
        self.assertEqual([r["score"] for r in rows], [300, 100])
        self.assertEqual(len(self.request("/api/scores?gameId=dino")[1]["scores"]), 1)

    def test_week_filter_applies_before_best_score_selection(self):
        self.request("/api/scores", self.score("old", score=1000))
        self.request("/api/scores", self.score("new", score=20))
        with self.store.db() as con:
            con.execute("UPDATE scores SET created_at='2000-01-01T00:00:00Z' WHERE id='old'")
            con.commit()
        self.assertEqual(self.request("/api/scores?period=week")[1]["scores"][0]["score"], 20)

    def test_profile_and_scores_survive_store_restart(self):
        self.request("/api/profile", {"playerName": "나이트 플레이어"})
        self.request("/api/scores", self.score())
        reopened = Store(self.root)
        self.assertEqual(reopened.profile()["playerName"], "나이트 플레이어")
        self.assertEqual(len(reopened.scores()), 1)

    def test_atomic_inbox_skips_incomplete_and_imports_once(self):
        self.store.profile("실제 플레이어")
        payload = {"eventId": "native-1", "gameId": "dino", "score": 345, "practice": False}
        path = self.store.outbox / "round.json.tmp"
        path.write_text(json.dumps(payload), encoding="utf-8")
        self.store.ingest()
        self.assertFalse(self.store.scores())
        complete = path.with_suffix("")
        path.rename(complete)
        self.store.ingest()
        complete.write_text(json.dumps(payload), encoding="utf-8")
        self.store.ingest()
        rows = self.request("/api/scores")[1]["scores"]
        self.assertEqual((len(rows), rows[0]["playerName"], rows[0]["source"]), (1, "실제 플레이어", "program"))

    def test_practice_and_malformed_results_do_not_block_valid_records(self):
        for name, data in (("practice", self.score("practice", practice=True)), ("valid", self.score("valid"))):
            (self.store.outbox / (name + ".json")).write_text(json.dumps(data), encoding="utf-8")
        (self.store.outbox / "bad.json").write_text("invalid", encoding="utf-8")
        self.store.ingest()
        self.assertEqual(len(self.store.scores()), 1)
        self.assertTrue((self.store.outbox / "bad.rejected").exists())

    def test_untrusted_origins_cannot_write(self):
        self.assertEqual(self.request("/api/scores", self.score(), {"Origin": "https://untrusted.example"})[0], 403)
        self.assertFalse(self.store.scores())

    def test_unrelated_hostnames_are_rejected(self):
        self.assertEqual(self.request("/api/health", headers={"Host": "unrelated.example"})[0], 403)

    def test_localhost_and_loopback_origins_can_connect(self):
        origin = f"http://localhost:{self.server.server_port}"
        self.assertEqual(self.request("/api/profile", {"playerName": "로컬"}, {"Origin": origin})[0], 200)

    def test_trusted_cross_origin_preflight(self):
        status, _, headers = self.request("/api/scores", headers={"Origin": "https://arena.example", "Access-Control-Request-Private-Network": "true"}, method="OPTIONS")
        self.assertEqual(status, 204)
        self.assertEqual(headers["Access-Control-Allow-Origin"], "https://arena.example")
        self.assertEqual(headers["Access-Control-Allow-Private-Network"], "true")

    def test_native_program_outbox_to_http_ranking(self):
        executable = os.environ.get("LOTTOL_TEST_OUTBOX_EXE")
        if not executable:
            self.skipTest("Set LOTTOL_TEST_OUTBOX_EXE to compiled score_outbox_smoke.exe")
        subprocess.run([executable, str(self.store.outbox)], check=True, capture_output=True)
        self.assertEqual(len(list(self.store.outbox.glob("*.json"))), 2)
        for path in self.store.outbox.glob("*.json"):
            self.assertIn("recordedAtMs", json.loads(path.read_text(encoding="utf-8")))
        # Offline import keeps the name entered when that game started.
        self.store.profile("다음 플레이어")
        self.store.ingest()
        rows = self.request("/api/scores")[1]["scores"]
        self.assertEqual([r["score"] for r in rows], [5600, 1234])
        self.assertTrue(all(r["source"] == "program" for r in rows))
        self.assertEqual([r["playerName"] for r in rows], ["민수", '한별 "1"\\A'])

    def test_actual_frontend_client_can_connect_and_submit(self):
        node = shutil.which("node")
        if not node:
            self.skipTest("Node.js required for frontend API integration test")
        result = subprocess.run([node, str(ROOT / "server/client_integration_test.js"), self.base], capture_output=True, text=True, encoding="utf-8", timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_page_controller_connects_renders_and_recovers_without_a_browser(self):
        node = shutil.which("node")
        if not node:
            self.skipTest("Node.js required")
        result = subprocess.run([node, str(ROOT / "server/page_integration_test.js"), self.base], capture_output=True, text=True, encoding="utf-8", timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
