#!/usr/bin/env python3

import argparse
import json
import os
import signal
import subprocess
import sys
import threading
import time
from datetime import datetime
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import urlparse
from urllib.request import Request, urlopen

APP_DIR = Path(__file__).resolve().parent
WARGAMES_DIR = APP_DIR.parent
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8765

SYSTEM_PROMPT_FILE = APP_DIR / "wopr-system-prompt.txt"
MEMORY_FILE = APP_DIR / "wopr-memory.txt"
MEMORY_LOG_FILE = APP_DIR / "wopr-memory.log"
LLM_CONFIG_FILE = APP_DIR / "wopr-llm.json"

# Optional macOS speech output for Joshua. Set TTS_ENABLED = False to mute him.
TTS_ENABLED = True
TTS_VOICE = "Evan (Enhanced)"
TTS_RATE = 155

DEFAULT_LLM_CONFIG = {
    "provider": 'local_llamacpp',
    "api_base": 'http://127.0.0.1:8766',
    "model": 'gemma-4-E4B-it-UD-Q4_K_XL',
    "temperature": 0.65,
    "top_p": 0.9,
    "max_tokens": 1024,
    "context_size": 32768,
    "api_key": '',
    "local_llamacpp": {
        "server_path": '../llama.cpp/build/bin/llama-server',
        "model_path": '../models/gemma-4-E4B-it-UD-Q4_K_XL.gguf',
        "host": '127.0.0.1',
        "port": 8766,
        "gpu_layers": 999
    }
}

def load_llm_config():
    if not LLM_CONFIG_FILE.is_file():
        LLM_CONFIG_FILE.write_text(json.dumps(DEFAULT_LLM_CONFIG, indent=2) + "\n", encoding="utf-8")
        return dict(DEFAULT_LLM_CONFIG)
    try:
        loaded = json.loads(LLM_CONFIG_FILE.read_text(encoding="utf-8"))
    except Exception as exc:
        raise RuntimeError(f"Unable to read {LLM_CONFIG_FILE}: {exc}") from exc
    config = dict(DEFAULT_LLM_CONFIG)
    config.update(loaded)
    local = dict(DEFAULT_LLM_CONFIG["local_llamacpp"])
    local.update(loaded.get("local_llamacpp", {}))
    config["local_llamacpp"] = local
    return config

LLM_CONFIG = load_llm_config()
MODEL_NAME = str(LLM_CONFIG["model"])
MEMORY_INTERVAL_TURNS = 1
MAX_MEMORY_SUMMARIES = 25
MAX_MEMORY_CHARS = 12000

CONFIG = {
    "service": "wargames-wopr-chat",
    "engine": LLM_CONFIG["provider"],
    "model": MODEL_NAME,
    "api_keys_required": bool(LLM_CONFIG.get("api_key")),
    "inference_location": "local" if LLM_CONFIG["provider"] == "local_llamacpp" else "configured",
    "terminal_bridge": True,
}

state_lock = threading.Lock()
messages = []
recent_memory_turns = []
memory_turn_count = 0
llama_process = None


def read_text(path, default=""):
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return default


def load_persistent_memory():
    return read_text(MEMORY_FILE).strip()


def memory_log(message):
    """Append a timestamped memory diagnostic entry to wopr-memory.log."""
    try:
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        with MEMORY_LOG_FILE.open("a", encoding="utf-8") as log:
            log.write(f"[{timestamp}] {message}\n")
    except Exception as exc:
        print(f"Memory log write failed: {exc}", file=sys.stderr)


def build_effective_system_prompt():
    system_prompt = read_text(SYSTEM_PROMPT_FILE).strip()
    memory = load_persistent_memory()

    if not memory:
        return system_prompt

    return f"""{system_prompt}

PERSISTENT MEMORY
These are compressed factual memories from earlier conversations.
Use them as background facts.
CORE IDENTITY and the IDENTITY MAP in the main system prompt always override memory.
Do not quote this section unless directly asked.
Do not assume anything beyond what is written here.

{memory}"""


def reset_conversation():
    global messages, recent_memory_turns, memory_turn_count
    with state_lock:
        messages = [{"role": "system", "content": build_effective_system_prompt()}]
        recent_memory_turns = []
        memory_turn_count = 0


def get_json(url, timeout=1.0):
    try:
        with urlopen(url, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))
    except Exception:
        return None


def resolve_config_path(value):
    path = Path(str(value))
    return path if path.is_absolute() else (APP_DIR / path).resolve()


def api_base():
    base = str(LLM_CONFIG["api_base"]).strip().rstrip("/")
    if base.lower().endswith("/v1"):
        base = base[:-3].rstrip("/")
    return base


def llama_ready():
    if LLM_CONFIG["provider"] != "local_llamacpp":
        return bool(api_base())
    return get_json(f"{api_base()}/health", timeout=0.5) is not None


def start_llama_server():
    global llama_process
    if LLM_CONFIG["provider"] != "local_llamacpp":
        return True
    if llama_ready():
        return True

    local = LLM_CONFIG["local_llamacpp"]
    llama_server = resolve_config_path(local["server_path"])
    model_file = resolve_config_path(local["model_path"])

    if not llama_server.is_file():
        print(f"llama-server not found: {llama_server}", file=sys.stderr)
        return False
    if not model_file.is_file():
        print(f"WOPR model not found: {model_file}", file=sys.stderr)
        return False

    cmd = [
        str(llama_server), "-m", str(model_file),
        "--host", str(local["host"]), "--port", str(local["port"]),
        "-c", str(LLM_CONFIG.get("context_size", 4096)),
        "-ngl", str(local.get("gpu_layers", 999)),
        "--reasoning", "off",
        "--reasoning-budget", "0",
        "--no-webui",
    ]

    log_path = APP_DIR / "llama-server.log"
    log = open(log_path, "a", encoding="utf-8")
    llama_process = subprocess.Popen(
        cmd, stdout=log, stderr=subprocess.STDOUT, cwd=str(WARGAMES_DIR)
    )

    for _ in range(600):
        if llama_process.poll() is not None:
            print(f"llama-server exited during startup. See {log_path}", file=sys.stderr)
            return False
        if llama_ready():
            return True
        time.sleep(0.1)

    print(f"Timed out waiting for llama-server. See {log_path}", file=sys.stderr)
    return False


def stop_llama_server():
    global llama_process
    if LLM_CONFIG["provider"] != "local_llamacpp":
        return
    if llama_process is None or llama_process.poll() is not None:
        return
    llama_process.terminate()
    try:
        llama_process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        llama_process.kill()
        llama_process.wait(timeout=2)


def llama_chat(request_messages, temperature=None, top_p=None, max_tokens=None):
    payload = json.dumps({
        "model": MODEL_NAME,
        "messages": request_messages,
        "temperature": LLM_CONFIG.get("temperature", 0.65) if temperature is None else temperature,
        "top_p": LLM_CONFIG.get("top_p", 0.9) if top_p is None else top_p,
        "max_tokens": LLM_CONFIG.get("max_tokens", 220) if max_tokens is None else max_tokens,
        "stream": False,
    }).encode("utf-8")

    headers = {"Content-Type": "application/json"}
    api_key = str(LLM_CONFIG.get("api_key", "")).strip()
    if api_key:
        headers["Authorization"] = f"Bearer {api_key}"

    request = Request(
        f"{api_base()}/v1/chat/completions",
        data=payload, headers=headers, method="POST",
    )
    with urlopen(request, timeout=600) as response:
        data = json.loads(response.read().decode("utf-8"))
    return str(data["choices"][0]["message"]["content"]).strip()

def save_memory_summary(summary):
    summary = summary.strip()
    if not summary:
        return

    existing = load_persistent_memory()
    blocks = [b.strip() for b in existing.split("\n\n") if b.strip()]
    blocks.append(f"[{datetime.now().strftime('%Y-%m-%d %H:%M')}]\n{summary}")
    blocks = blocks[-MAX_MEMORY_SUMMARIES:]

    while blocks and len("\n\n".join(blocks)) > MAX_MEMORY_CHARS:
        blocks.pop(0)

    MEMORY_FILE.write_text("\n\n".join(blocks) + "\n", encoding="utf-8")


def summarise_recent_memory(turns):
    recent_text = "\n".join(
        f"{turn['role'].upper()}: {turn['content']}" for turn in turns
    )
    persistent = load_persistent_memory() or "(none)"

    prompt = f"""Extract only NEW durable facts worth remembering across future sessions.

KEEP:
- new names or facts about people other than the fixed WOPR/Falken identity
- preferences
- ongoing tasks or projects
- decisions
- important events
- facts the user explicitly asks you to remember

NEVER STORE THESE FIXED IDENTITY FACTS:
- WOPR is Joshua
- Joshua is WOPR
- the user is Professor Stephen W. Falken
- Professor Falken created WOPR/Joshua
- WOPR/Joshua is not Professor Falken

DISCARD:
- greetings
- conversational filler
- temporary mood unless clearly important
- repeated facts already present in existing memory
- speculation
- statements generated by WOPR about its own implementation or operating state

RULES:
- Do not invent anything.
- Output ONLY the memory sentences themselves.
- Do not write headings.
- Do not use bullet points.
- Maximum 70 words.
- Heavy compression.
- If there is nothing new and durable to remember, output exactly: NO_NEW_MEMORY

EXISTING MEMORY:
{persistent}

RECENT CONVERSATION:
{recent_text}"""

    memory_log(f"sending {len(turns)} messages to summariser")
    summary = llama_chat(
        [
            {
                "role": "system",
                "content": "You are a memory compression process. Extract durable factual memory only.",
            },
            {"role": "user", "content": prompt},
        ],
        temperature=0.1,
        top_p=0.9,
        max_tokens=120,
    ).strip()

    memory_log(f"summariser result: {summary!r}")

    if summary.upper() == "NO_NEW_MEMORY":
        memory_log("nothing new to store")
        return

    summary = summary.replace("NO_NEW_MEMORY", "").strip()
    if summary:
        save_memory_summary(summary)
        memory_log(f"saved to {MEMORY_FILE}")



def speak_reply(text):
    """Speak Joshua's reply asynchronously on macOS without delaying the terminal."""
    if not TTS_ENABLED or not text or sys.platform != "darwin":
        return

    # Collapse control metadata if it is ever reintroduced into replies.
    spoken = str(text).split("<WOPR_CONTROL>", 1)[0].strip()
    if not spoken:
        return

    def _speak():
        try:
            subprocess.run(
                ["say", "-v", TTS_VOICE, "-r", str(TTS_RATE), spoken],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=False,
            )
        except Exception as exc:
            print(f"WOPR TTS failed: {exc}", file=sys.stderr)

    threading.Thread(target=_speak, daemon=True).start()


def generate_reply(user_text):
    global memory_turn_count, recent_memory_turns, messages

    with state_lock:
        messages.append({"role": "user", "content": user_text})
        request_messages = list(messages)

    reply = llama_chat(request_messages)
    speak_reply(reply)

    turns_to_summarise = None
    with state_lock:
        messages.append({"role": "assistant", "content": reply})
        recent_memory_turns.extend([
            {"role": "user", "content": user_text},
            {"role": "assistant", "content": reply},
        ])
        memory_turn_count += 1

        if memory_turn_count >= MEMORY_INTERVAL_TURNS:
            turns_to_summarise = list(recent_memory_turns)
            recent_memory_turns = []
            memory_turn_count = 0

    if turns_to_summarise:
        try:
            memory_log(
                f"interval reached ({MEMORY_INTERVAL_TURNS} exchange(s)); "
                f"processing {len(turns_to_summarise)} messages"
            )
            summarise_recent_memory(turns_to_summarise)
            memory_log("consolidation completed")
            # Refresh the system message so new memory is available immediately.
            with state_lock:
                if messages and messages[0]["role"] == "system":
                    messages[0]["content"] = build_effective_system_prompt()
        except Exception as exc:
            memory_log(f"ERROR: memory summarisation failed: {exc}")
            print(f"Memory summarisation failed: {exc}", file=sys.stderr)

    return reply or "NO RESPONSE."


class WOPRHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print("[wopr-chat] " + (fmt % args))

    def _json(self, payload, status=200):
        body = json.dumps(payload, indent=2).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _read_json(self):
        try:
            length = int(self.headers.get("Content-Length", "0"))
            raw = self.rfile.read(length) if length else b"{}"
            return json.loads(raw.decode("utf-8"))
        except Exception:
            return {}

    def do_GET(self):
        path = urlparse(self.path).path

        if path == "/health":
            self._json({"ok": True, "llm_ready": llama_ready(), "llama_ready": llama_ready(), **CONFIG})
            return

        if path == "/config":
            self._json(CONFIG)
            return

        if path == "/api/status":
            self._json({"ok": True, "llm_ready": llama_ready(), "llama_ready": llama_ready(), **CONFIG})
            return

        if path == "/api/memory":
            self._json({"ok": True, "memory": load_persistent_memory()})
            return

        self._json({"ok": False, "error": "unknown endpoint"}, status=404)

    def do_POST(self):
        path = urlparse(self.path).path
        data = self._read_json()

        if path == "/api/query":
            prompt = str(data.get("prompt", "")).strip()
            if not prompt:
                self._json({"ok": False, "error": "empty prompt"}, status=400)
                return

            try:
                reply = generate_reply(prompt)
                self._json({"ok": True, "response": reply})
            except Exception as exc:
                self._json({"ok": False, "error": str(exc)}, status=500)
            return

        if path == "/api/memory/clear":
            MEMORY_FILE.write_text("", encoding="utf-8")
            reset_conversation()
            self._json({"ok": True})
            return

        if path == "/api/shutdown":
            self._json({"ok": True, "message": "WOPR Chat service shutting down"})
            threading.Thread(target=self.server.shutdown, daemon=True).start()
            return

        self._json({"ok": False, "error": "unknown endpoint"}, status=404)


def ensure_service(host, port):
    root = f"http://{host}:{port}"
    health = get_json(f"{root}/health")

    if health and health.get("ok"):
        return 0

    subprocess.Popen(
        [
            sys.executable,
            str(Path(__file__).resolve()),
            "--host", host,
            "--port", str(port),
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )

    for _ in range(600):
        time.sleep(0.1)
        health = get_json(f"{root}/health")
        if health and health.get("ok") and health.get("llm_ready", health.get("llama_ready")):
            return 0

    print("Unable to start WOPR Chat service.", file=sys.stderr)
    return 1


def stop_service(host, port):
    root = f"http://{host}:{port}"
    health = get_json(f"{root}/health")

    if not health:
        print("WOPR Chat service is not running.")
        return 0

    try:
        request = Request(
            f"{root}/api/shutdown",
            data=b"{}",
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urlopen(request, timeout=2.0) as response:
            response.read()
    except Exception as exc:
        print(f"Unable to stop WOPR llama.cpp service: {exc}", file=sys.stderr)
        return 1

    for _ in range(50):
        time.sleep(0.1)
        if not get_json(f"{root}/health", timeout=0.2):
            print("WOPR Chat service stopped.")
            return 0

    return 1


def main():
    parser = argparse.ArgumentParser(
        description="Wargames WOPR conversational bridge for local llama.cpp or an OpenAI-compatible endpoint."
    )
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--ensure", action="store_true")
    parser.add_argument("--stop", action="store_true")
    args = parser.parse_args()

    if args.ensure and args.stop:
        parser.error("--ensure and --stop cannot be used together")

    if args.ensure:
        raise SystemExit(ensure_service(args.host, args.port))

    if args.stop:
        raise SystemExit(stop_service(args.host, args.port))

    if not start_llama_server():
        raise SystemExit(1)

    reset_conversation()
    server = ThreadingHTTPServer((args.host, args.port), WOPRHandler)

    print("WOPR Chat service")
    print("----------------------")
    print(f"Bridge   : http://{args.host}:{args.port}")
    print(f"Provider : {LLM_CONFIG['provider']}")
    print(f"LLM      : {api_base()}")
    print(f"Model    : {MODEL_NAME}")
    if TTS_ENABLED and sys.platform == "darwin":
        print(f"Voice    : {TTS_VOICE} @ {TTS_RATE} wpm")
    else:
        print("Voice    : disabled")
    if LLM_CONFIG["provider"] == "local_llamacpp":
        print(f"Model file: {resolve_config_path(LLM_CONFIG['local_llamacpp']['model_path'])}")
    print("Press Ctrl+C to stop.")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down WOPR Chat service.")
    finally:
        server.server_close()
        stop_llama_server()


if __name__ == "__main__":
    main()
