/* Native Discord presence extension. GPL-3.0 with this combined mod.
   All RPC, configuration and polling run on a worker after DLL attach returns.
   No game function calls or writes are made by this feature. */
#include "discord_presence.h"
#include "presence_cars.h"
#include "presence_json.h"
#include "support.h"

#define PRESENCE_VERSION "0.1.0-rc2"
#define DEFAULT_APP_ID_W L"1559239981548052682"
#define RPC_CAPACITY 65536u
#define TEXT_CAPACITY 4096u
#ifdef FH2_PRESENCE_TEST
/* Accelerated clocks for the same worker loop against a private mock pipe. */
#define RPC_POLL_MS 10u
#define GAME_POLL_MS 50u
#define UPDATE_INTERVAL_MS 250u
#define RECONNECT_MS 150u
#define HANDSHAKE_TIMEOUT_MS 1000u
#define ACK_TIMEOUT_MS 1000u
static const wchar_t *test_pipe_name;
static volatile LONG test_stop;
#else
#define RPC_POLL_MS 100u
#define GAME_POLL_MS 2000u
#define UPDATE_INTERVAL_MS 15000u
#define RECONNECT_MS 10000u
#define HANDSHAKE_TIMEOUT_MS 5000u
#define ACK_TIMEOUT_MS 10000u
#endif
#define MAX_LOG_BYTES (256u * 1024u)
#define MAX_SCENE_CARS 64u

/* Supported FH2 Xbox One build 1.0.0.64 only; verified before startup. */
#define RVA_GAME 0x34aba60u
#define RVA_PLAYER_MANAGER 0x34b3e18u
#define RVA_PLAYER_MANAGER_VTABLE 0x2c62a40u
#define RVA_CAR_VTABLE 0x2c5cb80u
#define RVA_HUD 0x34c0f40u
#define RVA_PRESENCE_VTABLE 0x2cbb130u

typedef struct {
    int loaded, paused, car_id, activity, has_position;
    float x, z;
} PresenceSample;
typedef struct { char bytes[TEXT_CAPACITY]; size_t used; int failed; } JsonBuffer;
typedef struct { const char *name; double x, z; } Destination;
static const Destination destinations[] = {
    {"Nice Mass\xc3\xa9na", -821, -6061}, {"Castelletto", 9941, -7896},
    {"Sisteron", -3532, -2308}, {"Saint-Martin", 2241, -3100},
    {"Montellino", 8831, -3573}, {"San Giovanni", 5111, -3528}
};
static const char *activity_names[] = {
    "In game", "At the title screen", "Free roam", "On a road trip", "Racing",
    "Street racing", "Cross-country racing", "Showcase event", "Showcase event",
    "Bucket list challenge", "Photo mode", "In the Autoshow", "In the paint shop",
    "In the garage", "Online road trip", "Online free roam", "Playing King",
    "Playing Infected", "Online racing", "Online bucket list", "Online wager",
    "At a car meet", "Driving on Storm Island", "Storm Island free roam",
    "Storm Island sprint", "Storm Island circuit", "Storm Island cross-country sprint",
    "Storm Island cross-country circuit", "Storm Island Gauntlet", "Storm Island bucket list",
    "Storm Island online free roam", "Storm Island online road trip", "Storm Island King",
    "Storm Island Infected", "Storm Island online race", "Storm Island online bucket list",
    "Storm Island online wager", "Storm Island car meet"
};
static unsigned char *game_image;
static wchar_t ini_path[PATH_CAPACITY], log_path[PATH_CAPACITY];
static char app_id[64], large_image[128], small_image[128];
static int show_paused = 1, show_location = 1;
static char receive_buffer[RPC_CAPACITY], previous_activity[TEXT_CAPACITY];
static size_t receive_used;
static ULONGLONG nonce;
static char pending_nonce[32];
static ULONGLONG pending_since;
static HANDLE io_event;
static char last_log[512];

static void put(JsonBuffer *b, char c) {
    if (b->used + 1 >= sizeof(b->bytes)) { b->failed = 1; return; }
    b->bytes[b->used++] = c; b->bytes[b->used] = 0;
}
static void append(JsonBuffer *b, const char *s) { while (*s) put(b, *s++); }
static void number(JsonBuffer *b, ULONGLONG value) {
    char digits[32]; unsigned n = 0;
    do { digits[n++] = (char)('0' + value % 10); value /= 10; } while (value);
    while (n) put(b, digits[--n]);
}
static void quoted(JsonBuffer *b, const char *s) {
    static const char hex[] = "0123456789abcdef";
    put(b, '"');
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        if (c == '"' || c == '\\') { put(b, '\\'); put(b, (char)c); }
        else if (c < 32) { append(b, "\\u00"); put(b, hex[c >> 4]); put(b, hex[c & 15]); }
        else put(b, (char)c);
    }
    put(b, '"');
}
static void copy_text(char *dst, size_t capacity, const char *src) {
    if (!capacity) return;
    size_t i = 0; while (i + 1 < capacity && src[i]) { dst[i] = src[i]; ++i; } dst[i] = 0;
}
static int equal_text(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; } return *a == *b;
}
static void report(const char *message) {
    if (equal_text(message, last_log)) return;
    HANDLE f = CreateFileW(log_path, FILE_APPEND_DATA | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (f == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(f, &size) || size.QuadPart >= MAX_LOG_BYTES) { CloseHandle(f); return; }
    copy_text(last_log, sizeof(last_log), message);
    DWORD written;
    WriteFile(f, message, (DWORD)text_length(message), &written, 0);
    WriteFile(f, "\r\n", 2, &written, 0); CloseHandle(f);
}
static int ptr(const void *address, unsigned char **value) {
    return memory_read(address, value, sizeof(*value)) && *value;
}
static int same_vtable(const unsigned char *obj, unsigned rva) {
    unsigned char *table; return obj && ptr(obj, &table) && table == game_image + rva;
}
static unsigned char *local_player(void) {
    unsigned char *manager, *head, *node, *selected = 0, first;
    size_t count;
    if (!ptr(game_image + RVA_PLAYER_MANAGER, &manager) || !same_vtable(manager, RVA_PLAYER_MANAGER_VTABLE) ||
        !memory_read(manager + 0x34, &first, 1) || !memory_read(manager + 0x40, &count, sizeof(count)) ||
        count < (first ? 1u : 2u) || count > 16 || !ptr(manager + 0x38, &head) ||
        !ptr(head, &node) || node == head) return 0;
    if (!first && (!ptr(node, &node) || node == head)) return 0;
    ptr(node + 0x10, &selected);
    return selected;
}

static PresenceSample sample_game(void) {
    PresenceSample s = {0}; s.activity = -1;
    unsigned char *game, *simulation, *scene, *begin, *end;
    if (!game_image || !ptr(game_image + RVA_GAME, &game) || !ptr(game + 0x48, &simulation) ||
        !ptr(simulation + 0x98, &scene)) return s;
    unsigned char paused;
    if (memory_read(simulation + 0x28, &paused, 1)) s.paused = paused != 0;
    if (!ptr(scene + 0x458, &begin) || !ptr(scene + 0x460, &end) ||
        (uintptr_t)end <= (uintptr_t)begin || (uintptr_t)end - (uintptr_t)begin > MAX_SCENE_CARS * 8 ||
        ((uintptr_t)end - (uintptr_t)begin) % 8) return s;
    unsigned char *selected = local_player();
    if (!selected) return s;
    /* Snapshot the vector in one guarded read. Recheck ownership before publish. */
    unsigned char *cars[MAX_SCENE_CARS];
    size_t car_bytes = (uintptr_t)end - (uintptr_t)begin;
    if (!memory_read(begin, cars, car_bytes)) return s;
    unsigned char *found = 0;
    for (size_t i = 0; i < car_bytes / sizeof(cars[0]); ++i) {
        unsigned char *car = cars[i], excluded; int index;
        if (!same_vtable(car, RVA_CAR_VTABLE) ||
            !memory_read(car + 0x4d08, &index, 4) || index ||
            !memory_read(car + 0x3f5, &excluded, 1) || excluded) continue;
        unsigned char *component = 0;
        if (!ptr(car + 0x4d00, &component) || component != selected) continue;
        if (found) return s; /* Ambiguous selection: do not publish a traffic/preview car. */
        found = car;
    }
    if (!found || !memory_read(found + 0x118, &s.car_id, 4) || s.car_id <= 0 ||
        s.car_id == 1215) return s; /* Internal NULL CAR used during transitions. */
    float xyz[3];
    if (memory_read(found + 0x4f50, xyz, sizeof(xyz)) && xyz[0] > -50000 && xyz[0] < 50000 &&
        xyz[1] > -10000 && xyz[1] < 10000 && xyz[2] > -50000 && xyz[2] < 50000) {
        s.x = xyz[0]; s.z = xyz[2]; s.has_position = 1;
    }
    unsigned char *hud, *services, *head, *node;
    if (ptr(game_image + RVA_HUD, &hud) && ptr(hud + 8, &services) &&
        ptr(services + 0xf8, &head) && ptr(head, &node)) {
        for (unsigned n = 0; node != head && n < 128; ++n) {
            unsigned char *owners, *holder, *wrapper, *obj;
            if (ptr(node + 0x20, &owners) && ptr(node + 0x18, &holder) && ptr(holder, &wrapper) &&
                ptr(wrapper + 0x10, &obj) && same_vtable(obj, RVA_PRESENCE_VTABLE)) {
                int activity;
                if (memory_read(obj + 0x70, &activity, 4) && activity >= 0 && activity < 38)
                    s.activity = activity;
                break;
            }
            if (!ptr(node, &node)) break;
        }
    }
    unsigned char *check;
    int car_id;
    if (!ptr(game_image + RVA_GAME, &check) || check != game ||
        !ptr(game + 0x48, &check) || check != simulation ||
        !ptr(simulation + 0x98, &check) || check != scene ||
        !ptr(scene + 0x458, &check) || check != begin ||
        !ptr(scene + 0x460, &check) || check != end ||
        local_player() != selected || !same_vtable(found, RVA_CAR_VTABLE) ||
        !ptr(found + 0x4d00, &check) || check != selected ||
        !memory_read(found + 0x118, &car_id, sizeof(car_id)) || car_id != s.car_id) return s;
    s.loaded = 1;
    return s;
}
static ULONGLONG unix_time(void) {
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULONGLONG value = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return value / 10000000 - 11644473600ULL;
}
static JsonBuffer activity_json(PresenceSample s, ULONGLONG started) {
    JsonBuffer b = {0}, details = {0}, car = {0};
    const char *mode = s.activity >= 0 && s.activity < 38 ? activity_names[s.activity] : "In game";
    if (show_paused && s.paused) mode = "Paused";
    append(&details, mode);
    if (show_location && s.activity >= 22 && s.activity < (int)ARRAY_COUNT(activity_names)) {
        if (show_paused && s.paused) append(&details, " | Storm Island");
    }
    else if (show_location && s.has_position) {
        unsigned best = 0; double distance = 1e100;
        for (unsigned i = 0; i < ARRAY_COUNT(destinations); ++i) {
            double dx = s.x - destinations[i].x, dz = s.z - destinations[i].z, d = dx * dx + dz * dz;
            if (d < distance) { best = i; distance = d; }
        }
        if (distance < 25000000) { append(&details, " | Near "); append(&details, destinations[best].name); }
        else append(&details, " | Exploring the countryside");
    }
    const char *name = 0;
    for (unsigned i = 0; i < ARRAY_COUNT(presence_cars); ++i)
        if (presence_cars[i].id == (unsigned)s.car_id) { name = presence_cars[i].name; break; }
    if (name) append(&car, name);
    else { append(&car, "Car #"); number(&car, s.car_id > 0 ? (unsigned)s.car_id : 0); }
    append(&b, "{\"type\":0,\"details\":"); quoted(&b, details.bytes);
    append(&b, ",\"state\":"); quoted(&b, car.bytes);
    append(&b, ",\"timestamps\":{\"start\":"); number(&b, started); put(&b, '}');
    if (large_image[0] || small_image[0]) {
        append(&b, ",\"assets\":{");
        if (large_image[0]) {
            append(&b, "\"large_image\":"); quoted(&b, large_image);
            append(&b, ",\"large_text\":\"Forza Horizon 2\"");
        }
        if (small_image[0]) {
            if (large_image[0]) put(&b, ',');
            append(&b, "\"small_image\":"); quoted(&b, small_image);
            append(&b, ",\"small_text\":\"Running through Azimuth\"");
        }
        put(&b, '}');
    }
    append(&b, ",\"instance\":false}"); return b;
}
static int transfer(HANDLE pipe, void *bytes, DWORD size, int writing) {
    OVERLAPPED ov = {0}; ov.hEvent = io_event; ResetEvent(io_event);
    DWORD done = 0;
    BOOL ok = writing ? WriteFile(pipe, bytes, size, &done, &ov) : ReadFile(pipe, bytes, size, &done, &ov);
    if (!ok) {
        if (GetLastError() != ERROR_IO_PENDING) return 0;
        if (WaitForSingleObject(io_event, 1000) != WAIT_OBJECT_0) {
            CancelIoEx(pipe, &ov); GetOverlappedResult(pipe, &ov, &done, TRUE); return 0;
        }
        if (!GetOverlappedResult(pipe, &ov, &done, FALSE)) return 0;
    }
    return done == size;
}
static int send_frame(HANDLE pipe, unsigned opcode, const char *body, DWORD size) {
    DWORD header[2] = {opcode, size};
    return transfer(pipe, header, 8, 1) && (!size || transfer(pipe, (void *)body, size, 1));
}
static int send_activity(HANDLE pipe, const char *activity) {
    if (pending_nonce[0]) return 0; /* Only one SET_ACTIVITY may be in flight. */
    JsonBuffer b = {0};
    append(&b, "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":"); number(&b, GetCurrentProcessId());
    append(&b, ",\"activity\":"); append(&b, activity ? activity : "null");
    append(&b, "},\"nonce\":\""); number(&b, ++nonce); append(&b, "\"}");
    if (b.failed || !send_frame(pipe, 1, b.bytes, (DWORD)b.used)) return 0;
    JsonBuffer id = {0}; number(&id, nonce);
    copy_text(pending_nonce, sizeof(pending_nonce), id.bytes);
    pending_since = GetTickCount64();
    return 1;
}
static int poll_rpc(HANDLE pipe, int *ready, int *accepted) {
    DWORD available = 0;
    if (!PeekNamedPipe(pipe, 0, 0, 0, &available, 0)) return 0;
    if (available) {
        size_t space = sizeof(receive_buffer) - receive_used;
        DWORD take = available < space ? available : (DWORD)space;
        if (!take || !transfer(pipe, receive_buffer + receive_used, take, 0)) return 0;
        receive_used += take;
    }
    while (receive_used >= 8) {
        DWORD header[2]; memory_copy(header, receive_buffer, 8);
        if (header[1] > sizeof(receive_buffer) - 8) return 0;
        size_t total = 8 + header[1];
        if (receive_used < total) break;
        char *body = receive_buffer + 8;
        if (header[0] == 0 || header[0] > 4) return 0;
        if (header[0] == 2) { report("PRESENCE: Discord closed or rejected the connection."); return 0; }
        if (header[0] == 3 && !send_frame(pipe, 4, body, header[1])) return 0;
        if (header[0] == 1) {
            char event[32], command[32], reply_nonce[32];
            if (presence_json_field(body, header[1], 0, "evt", event, sizeof(event)) < 0 ||
                presence_json_field(body, header[1], 0, "cmd", command, sizeof(command)) < 0 ||
                presence_json_field(body, header[1], 0, "nonce", reply_nonce, sizeof(reply_nonce)) < 0) {
                report("PRESENCE: invalid Discord reply; reconnecting."); return 0;
            }
            if (equal_text(event, "ERROR")) {
                char message[512];
                JsonBuffer error = {0}; append(&error, "PRESENCE: Discord error: ");
                if (presence_json_field(body, header[1], "data", "message", message, sizeof(message)) == 1) {
                    for (char *p = message; *p; ++p) if ((unsigned char)*p < 32) *p = ' ';
                    append(&error, message);
                } else append(&error, "request rejected (no usable message)");
                report(error.bytes); return 0;
            }
            if (equal_text(event, "READY") && equal_text(command, "DISPATCH")) *ready = 1;
            if (!event[0] && equal_text(command, "SET_ACTIVITY") && pending_nonce[0] &&
                equal_text(reply_nonce, pending_nonce)) { *accepted = 1; pending_nonce[0] = 0; }
        }
        /* Destination precedes source: forward copy is safe for this compaction. */
        for (size_t i = total; i < receive_used; ++i) receive_buffer[i - total] = receive_buffer[i];
        receive_used -= total;
    }
    return 1;
}
static int config_string(const wchar_t *key, const wchar_t *fallback, char *out, int capacity, int lower) {
    wchar_t value[256];
    DWORD length = GetPrivateProfileStringW(L"Discord", key, fallback, value, ARRAY_COUNT(value), ini_path);
    if (length == ARRAY_COUNT(value) - 1 ||
        !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, out, capacity, 0, 0)) return 0;
    if (lower) for (char *p = out; *p; ++p) if (*p >= 'A' && *p <= 'Z') *p += 'a' - 'A';
    return 1;
}
static int valid_application_id(const char *id) {
    ULONGLONG number = 0;
    if (!*id) return 0;
    for (; *id; ++id) {
        if (*id < '0' || *id > '9') return 0;
        unsigned digit = (unsigned)(*id - '0');
        if (number > (UINT64_MAX - digit) / 10) return 0;
        number = number * 10 + digit;
    }
    return number != 0;
}
static int config_boolean(const wchar_t *key, int fallback, int *out) {
    char value[8];
    if (!config_string(key, fallback ? L"1" : L"0", value, sizeof(value), 0)) return 0;
    if (equal_text(value, "0")) { *out = 0; return 1; }
    if (equal_text(value, "1")) { *out = 1; return 1; }
    return 0;
}
static DWORD WINAPI presence_worker(void *unused) {
    (void)unused;
    HANDLE log = CreateFileW(log_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    report("PRESENCE: " PRESENCE_VERSION " starting.");
    int enabled;
    if (!config_boolean(L"Enabled", 1, &enabled) ||
        !config_boolean(L"ShowPaused", 1, &show_paused) ||
        !config_boolean(L"ShowLocation", 1, &show_location)) {
        report("PRESENCE: Enabled, ShowPaused and ShowLocation must be 0 or 1; worker stopped."); return 0;
    }
    if (!enabled) {
        report("PRESENCE: disabled by configuration."); return 0;
    }
    if (!config_string(L"ApplicationId", DEFAULT_APP_ID_W, app_id, sizeof(app_id), 0) ||
        !config_string(L"LargeImage", L"fh2", large_image, sizeof(large_image), 1) ||
        !config_string(L"SmallImage", L"fh2-discord-artwork", small_image, sizeof(small_image), 1)) {
        report("PRESENCE: configuration text is too long or invalid; worker stopped."); return 0;
    }
    if (!valid_application_id(app_id)) { report("PRESENCE: invalid ApplicationId; worker stopped."); return 0; }
    io_event = CreateEventW(0, TRUE, FALSE, 0);
    if (!io_event) { report("PRESENCE: unable to create the IPC event."); return 0; }
    report("PRESENCE: native worker active; waiting for Discord and gameplay.");
    HANDLE pipe = INVALID_HANDLE_VALUE;
    ULONGLONG retry_at = 0, handshake_at = 0, last_send = 0, started = 0, next_sample = 0;
    PresenceSample sample = {0};
    int ready = 0, has_activity = 0, acknowledgement_logged = 0;
    for (;;) {
#ifdef FH2_PRESENCE_TEST
        if (InterlockedCompareExchange(&test_stop, 0, 0)) break;
#endif
        ULONGLONG now = GetTickCount64();
        if (now >= next_sample) {
            sample = sample_game();
            if (sample.loaded && !started) started = unix_time();
            next_sample = now + GAME_POLL_MS;
        }
        if (pipe == INVALID_HANDLE_VALUE && now >= retry_at) {
            wchar_t name[] = L"\\\\.\\pipe\\discord-ipc-0";
            for (unsigned i = 0; i < 10; ++i) {
                name[ARRAY_COUNT(name) - 2] = (wchar_t)(L'0' + i);
                const wchar_t *pipe_name = name;
#ifdef FH2_PRESENCE_TEST
                /* Test builds never open a real Discord endpoint. */
                if (!test_pipe_name) break;
                pipe_name = test_pipe_name;
#endif
                pipe = CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                                   FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, 0);
                if (pipe != INVALID_HANDLE_VALUE) break;
#ifdef FH2_PRESENCE_TEST
                break;
#endif
            }
            if (pipe == INVALID_HANDLE_VALUE) { report("PRESENCE: Discord desktop unavailable; retrying."); retry_at = now + RECONNECT_MS; }
            else {
                JsonBuffer handshake = {0}; append(&handshake, "{\"v\":1,\"client_id\":"); quoted(&handshake, app_id); put(&handshake, '}');
                receive_used = 0; ready = 0; has_activity = 0; acknowledgement_logged = 0;
                pending_nonce[0] = 0;
                previous_activity[0] = 0; last_send = 0; handshake_at = now;
                if (!send_frame(pipe, 0, handshake.bytes, (DWORD)handshake.used)) goto disconnected;
            }
        }
        if (pipe != INVALID_HANDLE_VALUE) {
            int accepted = 0;
            if (!poll_rpc(pipe, &ready, &accepted)) goto disconnected;
            if (!ready && now - handshake_at >= HANDSHAKE_TIMEOUT_MS) { report("PRESENCE: Discord handshake timed out."); goto disconnected; }
            if (pending_nonce[0] && now >= pending_since && now - pending_since >= ACK_TIMEOUT_MS) {
                report("PRESENCE: Discord activity acknowledgement timed out."); goto disconnected;
            }
            if (accepted && !acknowledgement_logged) {
                report("PRESENCE: Discord acknowledged SET_ACTIVITY."); acknowledgement_logged = 1;
            }
            if (ready && !pending_nonce[0] && (!last_send || now - last_send >= UPDATE_INTERVAL_MS)) {
                if (!sample.loaded) {
                    if (has_activity) {
                        if (!send_activity(pipe, 0)) goto disconnected;
                        has_activity = 0; previous_activity[0] = 0; last_send = now;
                    }
                } else {
                    JsonBuffer activity = activity_json(sample, started);
                    if (!activity.failed && !equal_text(activity.bytes, previous_activity)) {
                        if (!send_activity(pipe, activity.bytes)) goto disconnected;
                        copy_text(previous_activity, sizeof(previous_activity), activity.bytes); has_activity = 1; last_send = now;
                    }
                }
            }
        }
        Sleep(RPC_POLL_MS); continue;
disconnected:
        if (pipe != INVALID_HANDLE_VALUE) CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE; ready = 0; pending_nonce[0] = 0;
        retry_at = GetTickCount64() + RECONNECT_MS;
        report("PRESENCE: disconnected; retrying in 10 seconds.");
    }
    if (pipe != INVALID_HANDLE_VALUE) CloseHandle(pipe);
    CloseHandle(io_event); io_event = 0;
    return 0;
}
void discord_presence_start(unsigned char *image, const wchar_t *directory) {
    static volatile LONG launched;
    if (InterlockedCompareExchange(&launched, 1, 0)) return;
    game_image = image;
    static const unsigned char getter[] = {0x48,0x8b,0x49,0x48};
    static const unsigned char simulation_getter[] = {0x48,0x8b,0x81,0x98,0,0,0};
    unsigned char bytes[7];
    if (!path_join(ini_path, directory, L"FH2_DiscordPresence.ini") ||
        !path_join(log_path, directory, L"FH2_DiscordPresence.log")) return;
    if (!memory_read(image + 0x21e0c0, bytes, sizeof(getter)) || !memory_equal(bytes, getter, sizeof(getter)) ||
        !memory_read(image + 0x2e1520, bytes, sizeof(simulation_getter)) ||
        !memory_equal(bytes, simulation_getter, sizeof(simulation_getter))) {
        report("PRESENCE: unsupported game getter signatures; worker not started."); return;
    }
    HMODULE pinned;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            (LPCWSTR)(uintptr_t)presence_worker, &pinned)) {
        report("PRESENCE: unable to pin the DLL; worker not started."); return;
    }
    HANDLE worker = CreateThread(0, 0, presence_worker, 0, 0, 0);
    if (worker) CloseHandle(worker);
    else report("PRESENCE: unable to start the worker thread.");
}
#ifdef FH2_PRESENCE_TEST
__declspec(dllexport) void Fh2PresenceTestBind(void *image) { game_image = image; }
__declspec(dllexport) PresenceSample Fh2PresenceTestSample(void) { return sample_game(); }
__declspec(dllexport) int Fh2PresenceTestFormat(PresenceSample s, char *out, unsigned capacity) {
    copy_text(large_image, sizeof(large_image), "fh2"); small_image[0] = 0;
    JsonBuffer b = activity_json(s, 12345);
    if (b.failed || capacity <= b.used) return 0;
    copy_text(out, capacity, b.bytes); return 1;
}
__declspec(dllexport) int Fh2PresenceTestField(const char *json, const char *key, char *out, unsigned capacity) {
    return presence_json_field(json, text_length(json), 0, key, out, capacity);
}
__declspec(dllexport) int Fh2PresenceTestId(const char *id) { return valid_application_id(id); }
__declspec(dllexport) void Fh2PresenceTestStop(void) { InterlockedExchange(&test_stop, 1); }
__declspec(dllexport) int Fh2PresenceTestWorker(const wchar_t *directory, const wchar_t *pipe_name) {
    if (!path_join(ini_path, directory, L"FH2_DiscordPresence.ini") ||
        !path_join(log_path, directory, L"FH2_DiscordPresence.log")) return 0;
    test_pipe_name = pipe_name;
    InterlockedExchange(&test_stop, 0);
    pending_nonce[0] = 0; last_log[0] = 0;
    presence_worker(0);
    test_pipe_name = 0;
    return 1;
}
__declspec(dllexport) int Fh2PresenceTestSession(const wchar_t *name, const char *activity) {
    io_event = CreateEventW(0, TRUE, FALSE, 0);
    if (!io_event) return 0;
    HANDLE pipe = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                              FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, 0);
    if (pipe == INVALID_HANDLE_VALUE) { CloseHandle(io_event); return 0; }
    receive_used = 0;
    pending_nonce[0] = 0;
    int ready = 0, accepted = 0, success = 0;
    const char *handshake = "{\"v\":1,\"client_id\":\"1559239981548052682\"}";
    if (!send_frame(pipe, 0, handshake, (DWORD)text_length(handshake))) goto cleanup;
    ULONGLONG deadline = GetTickCount64() + 3000;
    while (!ready && GetTickCount64() < deadline) {
        if (!poll_rpc(pipe, &ready, &accepted)) goto cleanup;
        Sleep(10);
    }
    if (!ready || !send_activity(pipe, activity)) goto cleanup;
    deadline = GetTickCount64() + 3000;
    while (!accepted && GetTickCount64() < deadline) {
        if (!poll_rpc(pipe, &ready, &accepted)) goto cleanup;
        Sleep(10);
    }
    if (accepted && send_activity(pipe, 0)) success = 1;
cleanup:
    CloseHandle(pipe); CloseHandle(io_event); io_event = 0; return success;
}
#endif
