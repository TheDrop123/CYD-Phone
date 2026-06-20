# Nostr / Notr — Server Setup

## Server Access
- **Host**: `149.102.157.124` (Contabo VPS, Ubuntu 22.04)
- **SSH**: `root@149.102.157.124` / pw: `traubig11!`
- **Frontend**: `http://149.102.157.124:3001/`

## Deployed Services

| Service | Path | Port |
|---------|------|------|
| Nostr Relay (Node.js ESM) | `/opt/nostr-relay/` | `ws://149.102.157.124:7777` |
| API Server + Web Client (CommonJS) | `/opt/webuntis-api/` | `http://149.102.157.124:3001` |
| Relay Event Store | `/var/lib/nostr-relay/events.jsonl` | — |
| User/Group/Message Store | `/var/lib/webuntis-api/` | — |

## Manage Services

```bash
systemctl status|restart|stop|start nostr-relay webuntis-api
journalctl -u webuntis-api -n 50 --no-pager
```

## Deploy Changes

```bash
scp relay/index.js       root@149.102.157.124:/opt/nostr-relay/
scp api/index.js         root@149.102.157.124:/opt/webuntis-api/
scp api/public/index.html root@149.102.157.124:/opt/webuntis-api/
ssh root@149.102.157.124 'systemctl restart webuntis-api'
```

## API Endpoints (Port 3001)

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/health` | Health check |
| POST | `/api/register` | Create user (generates Nostr secp256k1 keypair) |
| POST | `/api/login` | Login by username (returns existing keys) |
| GET | `/api/users` | List all users |
| POST | `/api/groups` | Create group `{name, ownerPubkey}` |
| GET | `/api/groups?pubkey=X` | List groups for user |
| GET | `/api/groups/:id` | Group details w/ member info |
| POST | `/api/groups/:id/join` | Join group |
| POST | `/api/groups/:id/invite` | Add member by pubkey |
| POST | `/api/groups/:id/leave` | Leave group |
| POST | `/api/messages` | Send message (creates Nostr kind-1 event) |
| GET | `/api/messages?groupId=X` | Group messages |
| GET | `/api/messages?pubkey=X&otherPubkey=Y` | DMs |
| GET | `/webuntis/timetable?start=&end=` | School timetable |

## WebUntis Credentials

- **Server**: `coppi-gymnasium.webuntis.com`
- **School**: `coppi-gymnasium`
- **User**: `9b` / **Pass**: `BietL2024!`
- JSON-RPC at `/WebUntis/jsonrpc.do` — no `?school=` param on subdomain tenants

## Quirks

- Nostr relay is **ESM** (`"type": "module"`), API server is **CommonJS** — don't mix imports
- Messages are written to both the Nostr relay and local `messages.jsonl` (API reads from local)
- `ufw` is active — new ports need `ufw allow <port>/tcp`
- Services run as `nobody:nogroup` — pre-create dirs with correct ownership
- All event/timetable data lives on the remote Contabo server
