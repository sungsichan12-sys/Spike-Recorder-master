"""LOTTOL's local ranking API, static site and durable game-result inbox.

Python standard library only. Does not open a browser or any audio device.
"""
import argparse
import configparser
from contextlib import contextmanager
from datetime import datetime, timedelta, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import ipaddress
from pathlib import Path
import re
import sqlite3
import threading
import unicodedata
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parent.parent
MAX_BODY = 16384
ID = re.compile(r"[A-Za-z0-9_-]{1,128}\Z")


class Invalid(ValueError):
    pass


class Store:
    def __init__(self, root=ROOT, data_dir=None, outbox=None):
        self.root = Path(root).resolve()
        self.data_dir = Path(data_dir or self.root / "server/data").resolve()
        self.outbox = Path(outbox or self.root / "server/outbox").resolve()
        self.data_dir.mkdir(parents=True, exist_ok=True)
        self.outbox.mkdir(parents=True, exist_ok=True)
        self.path = self.data_dir / "rankings.sqlite3"
        with self.db() as con:
            con.execute("PRAGMA journal_mode=WAL")
            con.executescript("""
                CREATE TABLE IF NOT EXISTS scores (
                    id TEXT PRIMARY KEY, player_name TEXT NOT NULL,
                    player_key TEXT NOT NULL, game_id TEXT NOT NULL,
                    score INTEGER NOT NULL CHECK(score BETWEEN 0 AND 1000000000),
                    created_at TEXT NOT NULL, source TEXT NOT NULL);
                CREATE INDEX IF NOT EXISTS scores_game ON scores(game_id,score DESC);
                CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY,value TEXT NOT NULL);
                INSERT OR IGNORE INTO settings VALUES('playerName','PLAYER');
            """)
            con.commit()

    @contextmanager
    def db(self):
        con = sqlite3.connect(self.path, timeout=5)
        con.row_factory = sqlite3.Row
        try:
            yield con
        finally:
            con.close()

    def games(self):
        games = []
        for path in sorted((self.root / "Spike-Recorder-master/game").glob("*/game.ini")):
            # Manifests have bare key=value lines, optionally a section header.
            parser = configparser.ConfigParser(interpolation=None)
            try:
                contents = path.read_text(encoding="utf-8-sig")
                if not contents.lstrip().startswith("["):
                    contents = "[game]\n" + contents
                parser.read_string(contents)
                values = parser[parser.sections()[0]]
                game_id = values.get("id", "")
                if re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,63}", game_id):
                    games.append({"id": game_id, "name": values.get("title", game_id)})
            except (OSError, ValueError, configparser.Error):
                continue
        return games

    @staticmethod
    def name(value):
        if not isinstance(value, str):
            raise Invalid("플레이어 이름을 입력해 주세요.")
        value = unicodedata.normalize("NFC", value).strip()
        if not 1 <= len(value) <= 32 or any(ord(c) < 32 or ord(c) == 127 for c in value):
            raise Invalid("플레이어 이름은 1~32자로 입력해 주세요.")
        return value

    def profile(self, value=None):
        with self.db() as con:
            if value is not None:
                value = self.name(value)
                con.execute("UPDATE settings SET value=? WHERE key='playerName'", (value,))
                con.commit()
            return {"playerName": con.execute("SELECT value FROM settings WHERE key='playerName'").fetchone()[0]}

    def save(self, payload, source="manual"):
        if not isinstance(payload, dict):
            raise Invalid("JSON 객체가 필요합니다.")
        event_id = payload.get("eventId")
        if not isinstance(event_id, str) or not ID.fullmatch(event_id):
            raise Invalid("유효한 기록 ID가 필요합니다.")
        game = payload.get("gameId")
        if game not in {g["id"] for g in self.games()}:
            raise Invalid("설치된 게임을 선택해 주세요.")
        score = payload.get("score")
        if type(score) is not int or not 0 <= score <= 1000000000:
            raise Invalid("점수는 0~1,000,000,000 사이의 정수여야 합니다.")
        if source == "program" and payload.get("practice", False) is not False:
            return {"ignored": True, "reason": "practice"}, False
        name = self.name(payload.get("playerName", self.profile()["playerName"]))
        now = datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")
        if source == "program" and "recordedAtMs" in payload:
            recorded = payload["recordedAtMs"]
            if type(recorded) is not int or not 946684800000 <= recorded <= int(datetime.now(timezone.utc).timestamp() * 1000) + 300000:
                raise Invalid("프로그램 기록의 시간이 올바르지 않습니다.")
            now = datetime.fromtimestamp(recorded / 1000, timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")
        with self.db() as con:
            con.execute("BEGIN IMMEDIATE")
            existing = con.execute("SELECT * FROM scores WHERE id=?", (event_id,)).fetchone()
            if existing:
                if existing["game_id"] != game or existing["score"] != score or existing["source"] != source:
                    raise Invalid("이미 다른 기록에 사용된 ID입니다.")
                return self.record(existing), False
            con.execute("INSERT INTO scores VALUES(?,?,?,?,?,?,?)",
                        (event_id, name, name.casefold(), game, score, now, source))
            con.commit()
        return {"eventId": event_id, "playerName": name, "gameId": game,
                "score": score, "createdAt": now, "source": source}, True

    @staticmethod
    def record(row):
        return {"eventId": row["id"], "playerName": row["player_name"],
                "gameId": row["game_id"], "score": row["score"],
                "createdAt": row["created_at"], "source": row["source"]}

    def scores(self, game="all", period="all"):
        if game != "all" and game not in {g["id"] for g in self.games()}:
            raise Invalid("알 수 없는 게임입니다.")
        if period not in ("all", "week"):
            raise Invalid("알 수 없는 기간입니다.")
        cutoff = (datetime.now(timezone.utc) - timedelta(days=7)).isoformat().replace("+00:00", "Z")
        with self.db() as con:
            rows = con.execute("""
                WITH ranked AS (
                    SELECT *, ROW_NUMBER() OVER (
                        PARTITION BY player_key,game_id ORDER BY score DESC,created_at,id) AS position
                    FROM scores WHERE (?='all' OR game_id=?) AND (?='all' OR created_at>=?)
                ) SELECT * FROM ranked WHERE position=1
                  ORDER BY score DESC,created_at,id LIMIT 2000
                """, (game, game, period, cutoff)).fetchall()
        return [self.record(row) for row in rows]

    def ingest(self):
        # Consume only completed, atomically renamed events. Crash/restart after
        # SQLite commit is safe: the same event ID cannot create a second row.
        for path in sorted(self.outbox.glob("*.json"))[:100]:
            if path.is_symlink():
                continue
            try:
                if path.stat().st_size > MAX_BODY:
                    raise Invalid("Result exceeds size limit")
                data = json.loads(path.read_text(encoding="utf-8"))
                self.save(data, source="program")
                path.unlink()
            except (Invalid, ValueError, UnicodeError):
                # Keep malformed results for inspection without blocking others.
                path.replace(path.with_suffix(".rejected"))
            except OSError:
                continue


class RankingServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, store, allowed_origins=()):
        self.store = store
        self.allowed_origins = set(allowed_origins)
        self.stop_event = threading.Event()
        super().__init__(address, Handler)

    def ingest_loop(self):
        while not self.stop_event.wait(.25):
            try:
                self.store.ingest()
            except (OSError, sqlite3.Error) as error:
                print("Inbox retry:", error, flush=True)

    def server_close(self):
        self.stop_event.set()
        super().server_close()


class Handler(BaseHTTPRequestHandler):
    server_version = "LOTTOL/1.0"

    def host_allowed(self):
        try:
            host = urlsplit("http://" + self.headers.get("Host", "")).hostname
            if host in ("localhost", "127.0.0.1", "::1"):
                return True
            return self.server.server_address[0] == "0.0.0.0" and ipaddress.ip_address(host).is_private
        except (ValueError, TypeError):
            return False

    def origin_allowed(self):
        origin = self.headers.get("Origin")
        local = {f"http://{host}:{self.server.server_port}" for host in ("localhost", "127.0.0.1", "[::1]")}
        return not origin or origin in self.server.allowed_origins or origin in local or origin == "http://" + self.headers.get("Host", "")

    def reply(self, status, value, content_type="application/json; charset=utf-8"):
        body = json.dumps(value, ensure_ascii=False).encode("utf-8") if content_type.startswith("application/json") else value
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        origin = self.headers.get("Origin")
        if origin and self.origin_allowed():
            self.send_header("Access-Control-Allow-Origin", origin)
            self.send_header("Vary", "Origin")
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        if not self.host_allowed() or not self.origin_allowed():
            self.reply(403, {"error": "허용되지 않은 홈페이지 주소입니다."})
            return
        self.send_response(204)
        if self.headers.get("Origin"):
            self.send_header("Access-Control-Allow-Origin", self.headers["Origin"])
            self.send_header("Vary", "Origin")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Private-Network", "true")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self):
        if not self.host_allowed():
            self.reply(403, {"error": "로컬 서버 주소로 접속해 주세요."})
            return
        path = urlsplit(self.path)
        params = parse_qs(path.query)
        try:
            if path.path == "/api/health":
                self.reply(200, {"app": "lottol-ranking", "status": "ok", "version": 1})
            elif path.path == "/api/games":
                self.reply(200, {"games": self.server.store.games()})
            elif path.path == "/api/profile":
                self.reply(200, self.server.store.profile())
            elif path.path in ("/api/scores", "/ranking.json"):
                self.reply(200, {"scores": self.server.store.scores(params.get("gameId", ["all"])[0], params.get("period", ["all"])[0])})
            elif path.path in ("/", "/index.html", "/arena-config.js", "/arena-client.js", "/arena-cloud-client.js", "/arena-page.js"):
                name = "index.html" if path.path in ("/", "/index.html") else path.path.lstrip("/")
                target = self.server.store.root / name
                if not target.is_file():
                    self.reply(404, {"error": "페이지 파일이 없습니다."})
                    return
                self.reply(200, target.read_bytes(), "text/html; charset=utf-8" if name.endswith("html") else "text/javascript; charset=utf-8")
            else:
                self.reply(404, {"error": "주소를 찾을 수 없습니다."})
        except Invalid as error:
            self.reply(400, {"error": str(error)})
        except (OSError, sqlite3.Error):
            self.reply(503, {"error": "저장소를 사용할 수 없습니다. 잠시 후 다시 시도해 주세요."})

    def do_POST(self):
        if not self.host_allowed() or not self.origin_allowed():
            self.reply(403, {"error": "허용되지 않은 홈페이지 주소입니다."})
            return
        try:
            if self.headers.get("Content-Type", "").split(";")[0].strip() != "application/json":
                raise Invalid("application/json 요청이 필요합니다.")
            try:
                length = int(self.headers.get("Content-Length", "0"))
            except ValueError:
                raise Invalid("잘못된 요청 크기입니다.")
            if not 0 < length <= MAX_BODY:
                self.reply(413, {"error": "요청 크기가 너무 크거나 비어 있습니다."})
                return
            self.connection.settimeout(5)
            data = json.loads(self.rfile.read(length).decode("utf-8"))
            path = urlsplit(self.path).path
            if path == "/api/scores":
                record, created = self.server.store.save(data)
                self.reply(201 if created else 200, {"record": record, "duplicate": not created})
            elif path == "/api/profile":
                if not isinstance(data, dict):
                    raise Invalid("JSON 객체가 필요합니다.")
                self.reply(200, self.server.store.profile(self.server.store.name(data.get("playerName"))))
            else:
                self.reply(404, {"error": "주소를 찾을 수 없습니다."})
        except (Invalid, ValueError, UnicodeError) as error:
            self.reply(400, {"error": str(error)})
        except (OSError, sqlite3.Error):
            self.reply(503, {"error": "기록을 저장하지 못했습니다. 다시 시도해 주세요."})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--allow-origin", action="append", default=[])
    args = parser.parse_args()
    store = Store()
    server = RankingServer((args.host, args.port), store, args.allow_origin)
    threading.Thread(target=server.ingest_loop, daemon=True).start()
    print(f"LOTTOL ranking server: http://{args.host}:{server.server_port}/", flush=True)
    try:
        server.serve_forever(poll_interval=.25)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
