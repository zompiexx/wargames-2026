# Wargames WOPR Chat

WOPR Chat provides the local conversational AI layer for the Wargames WOPR/Joshua simulator.

It uses **llama.cpp / llama-server** for local inference and adds a small rolling persistent-memory layer so Joshua can retain selected durable facts across sessions.

## Architecture

- **Inference engine:** llama.cpp / llama-server
- **Default model:** `Llama-3.2-3B-Instruct-Q4_K_M.gguf`
- **Model source:** `bartowski/Llama-3.2-3B-Instruct-GGUF`
- **Context:** 4096 tokens
- **WOPR bridge:** port `8765`
- **llama-server API:** port `8766`

The old browser/WebGPU WebLLM worker is no longer required. `wopr_chat.py` starts `llama-server` when required and sends requests directly to its OpenAI-compatible `/v1/chat/completions` endpoint.

The model is stored separately in the repository-level `models/` directory so it can be replaced with another compatible GGUF model if desired.

## Files

The `wopr_chat/` directory contains:

- `wopr_chat.py` — WOPR bridge, conversation state, llama.cpp lifecycle and persistent-memory handling.
- `wopr_query.py` — small command-line query client used by the WOPR simulator.
- `wopr-system-prompt.txt` — Joshua/WOPR's static identity and behavioural instructions.
- `wopr-memory.txt` — rolling persistent memory.
- `WOPR-CHAT-README.md` — this document.

The llama.cpp source/build lives separately in `./llama.cpp/`, and GGUF models live in `./models/`.

## Persistent memory

Joshua has three context layers:

1. **System prompt**  
   Static identity and behaviour from `wopr-system-prompt.txt`.

2. **Current conversation**  
   Normal in-session conversational context held by the WOPR Chat bridge.

3. **Persistent memory**  
   A small rolling memory stored in `wopr-memory.txt` and reused across sessions.

Every five user turns, the local model performs a separate memory-compression pass over the recent conversation.

It keeps only durable information such as:

- names and relationships
- preferences
- ongoing tasks or projects
- decisions
- important events
- facts explicitly requested to be remembered

The bridge retains only the five newest memory summaries, capped at roughly 4000 characters total.

When a new conversation starts, these summaries are injected into the system context under `PERSISTENT MEMORY`.

This is intentionally **not** a vector database or full RAG system. It is a tiny rolling-summary memory intended to give Joshua lightweight continuity.

## Lifecycle

Start or ensure the WOPR Chat service is running:

```bash
python3 ./wopr_chat/wopr_chat.py --ensure
```

Stop it:

```bash
python3 ./wopr_chat/wopr_chat.py --stop
```

When started, WOPR Chat will ensure the local `llama-server` process is available and the configured GGUF model is loaded.

## Testing

Query Joshua directly:

```bash
echo "WHAT IS YOUR NAME?" | python3 ./wopr_chat/wopr_query.py
```

Check the WOPR bridge:

```bash
curl http://127.0.0.1:8765/health
```

Inspect persistent memory:

```bash
curl http://127.0.0.1:8765/api/memory
```

or:

```bash
cat ./wopr_chat/wopr-memory.txt
```

Clear persistent memory:

```bash
curl -X POST http://127.0.0.1:8765/api/memory/clear
```

## Persistent-memory test

1. Start Joshua.
2. Tell him a durable fact, for example:

   `Please remember that my favourite retro computer is the Sinclair QL.`

3. Have at least five user turns so a memory-summary cycle runs.
4. Inspect `./wopr_chat/wopr-memory.txt`.
5. Stop and restart Joshua.
6. Ask:

   `What is my favourite retro computer?`

If the fact was selected as durable memory, Joshua should be able to recall it after the restart.

## Model replacement

The default model is deliberately modest so the Wargames simulator remains practical on ordinary modern hardware.

Because models are kept in the repository-level `models/` directory, users can substitute another compatible GGUF model if they want a larger or more capable Joshua. The WOPR Chat configuration must be updated to point at the replacement model.

## Design principle

The Wargames C simulator does not need to know how inference is implemented. It talks to the WOPR Chat integration layer; WOPR Chat currently uses llama.cpp as its local inference engine.

This keeps the simulator, chatbot integration, inference engine and model assets cleanly separated.
