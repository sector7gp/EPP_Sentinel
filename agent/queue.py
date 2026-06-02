import sqlite3
import time
from pathlib import Path


class UploadQueue:
    def __init__(self, db_path: str):
        self.db_path = db_path
        Path(db_path).parent.mkdir(parents=True, exist_ok=True)
        self._init_db()

    def _conn(self) -> sqlite3.Connection:
        return sqlite3.connect(self.db_path)

    def _init_db(self) -> None:
        with self._conn() as conn:
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS queue (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    file_path TEXT NOT NULL,
                    stream_id TEXT,
                    status TEXT NOT NULL DEFAULT 'pending',
                    retries INTEGER NOT NULL DEFAULT 0,
                    created_at REAL NOT NULL,
                    last_attempt REAL
                )
                """
            )
            cols = {row[1] for row in conn.execute("PRAGMA table_info(queue)").fetchall()}
            if "stream_id" not in cols:
                conn.execute("ALTER TABLE queue ADD COLUMN stream_id TEXT")

    def enqueue(self, file_path: str, stream_id: str | None = None) -> int:
        with self._conn() as conn:
            cur = conn.execute(
                "INSERT INTO queue (file_path, stream_id, status, created_at) VALUES (?, ?, 'pending', ?)",
                (file_path, stream_id, time.time()),
            )
            return int(cur.lastrowid)

    def pending_items(self, limit: int = 10) -> list[tuple[int, str, int, str | None]]:
        with self._conn() as conn:
            rows = conn.execute(
                """
                SELECT id, file_path, retries, stream_id FROM queue
                WHERE status IN ('pending', 'failed')
                ORDER BY created_at ASC
                LIMIT ?
                """,
                (limit,),
            ).fetchall()
        return [(r[0], r[1], r[2], r[3]) for r in rows]

    def mark_uploading(self, item_id: int) -> None:
        with self._conn() as conn:
            conn.execute(
                "UPDATE queue SET status='uploading', last_attempt=? WHERE id=?",
                (time.time(), item_id),
            )

    def mark_acked(self, item_id: int) -> None:
        with self._conn() as conn:
            conn.execute("UPDATE queue SET status='acked' WHERE id=?", (item_id,))

    def mark_failed(self, item_id: int, retries: int) -> None:
        with self._conn() as conn:
            conn.execute(
                "UPDATE queue SET status='failed', retries=?, last_attempt=? WHERE id=?",
                (retries + 1, time.time(), item_id),
            )

    def stats(self) -> dict[str, int]:
        with self._conn() as conn:
            rows = conn.execute(
                "SELECT status, COUNT(*) FROM queue GROUP BY status"
            ).fetchall()
        return {status: count for status, count in rows}
