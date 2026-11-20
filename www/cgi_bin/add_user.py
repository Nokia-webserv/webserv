#!/usr/bin/env python3
"""
add_user.py - CGI script that handles a POST request and adds a user
to a SQLite database.
 
Expected form fields (application/x-www-form-urlencoded):
    username  - required, 3-32 chars: letters, digits, _ . -
    email     - required, basic format check
    password  - required, min 8 chars (stored as a salted PBKDF2 hash)
 
Example:
    curl -X POST http://localhost/cgi-bin/add_user.py \
         -d "username=alice&email=alice@example.com&password=s3cretpass"
"""
 
import hashlib
import json
import os
import re
import secrets
import sqlite3
import sys
from urllib.parse import parse_qs
 
# Keep the DB outside the web root in production.
DB_PATH = os.environ.get("USERS_DB")
MAX_BODY = 8 * 1024  # 8 KB is plenty for this form
 
USERNAME_RE = re.compile(r"^[A-Za-z0-9_.-]{3,32}$")
EMAIL_RE = re.compile(r"^[^@\s]+@[^@\s]+\.[^@\s]+$")
 
 
def respond(status, payload):
    """Write a JSON response with the given HTTP status and exit."""
    reasons = {
        201: "Created", 400: "Bad Request", 405: "Method Not Allowed",
        409: "Conflict", 411: "Length Required", 413: "Payload Too Large",
        500: "Internal Server Error",
    }
    sys.stdout.write(f"Status: {status} {reasons.get(status, '')}\r\n")
    sys.stdout.write("Content-Type: application/json\r\n")
    if status == 405:
        sys.stdout.write("Allow: POST\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.write(json.dumps(payload))
    sys.exit(0)
 
 
def hash_password(password):
    salt = secrets.token_bytes(16)
    digest = hashlib.pbkdf2_hmac("sha256", password.encode(), salt, 600_000)
    return f"pbkdf2_sha256$600000${salt.hex()}${digest.hex()}"
 
 
def read_post_data():
    try:
        length = int(os.environ.get("CONTENT_LENGTH", ""))
    except ValueError:
        respond(411, {"error": "Content-Length required"})
    if length > MAX_BODY:
        respond(413, {"error": "Request body too large"})
    body = sys.stdin.read(length)
    parsed = parse_qs(body, keep_blank_values=True)
    return {k: v[0].strip() for k, v in parsed.items()}
 
 
def init_db(conn):
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS users (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            username      TEXT NOT NULL UNIQUE,
            email         TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            created_at    TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        )
        """
    )
 
 
def main():
    if os.environ.get("REQUEST_METHOD", "").upper() != "POST":
        respond(405, {"error": "Only POST is allowed"})
 
    data = read_post_data()
    username = data.get("username", "")
    email = data.get("email", "")
    password = data.get("password", "")
 
    errors = {}
    if not USERNAME_RE.match(username):
        errors["username"] = "3-32 chars; letters, digits, _ . - only"
    if not EMAIL_RE.match(email) or len(email) > 254:
        errors["email"] = "Invalid email address"
    if len(password) < 8:
        errors["password"] = "Must be at least 8 characters"
    if errors:
        respond(400, {"errors": errors})
 
    try:
        conn = sqlite3.connect(DB_PATH, timeout=10)
        try:
            init_db(conn)
            # Parameterized query: protects against SQL injection.
            cur = conn.execute(
                "INSERT INTO users (username, email, password_hash) VALUES (?, ?, ?)",
                (username, email, hash_password(password)),
            )
            conn.commit()
            user_id = cur.lastrowid
        finally:
            conn.close()
    except sqlite3.IntegrityError:
        respond(409, {"error": "Username or email already exists"})
    except sqlite3.Error as exc:
        # Log details server-side, don't leak them to the client.
        print(f"add_user.py DB error: {exc}", file=sys.stderr)
        respond(500, {"error": "Database error"})
 
    respond(201, {"id": user_id, "username": username, "email": email})
 
 
if __name__ == "__main__":
    main()
 

