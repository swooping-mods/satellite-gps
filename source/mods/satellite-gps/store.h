#ifndef MOONLAB_MAPPING_STORE_H
#define MOONLAB_MAPPING_STORE_H
/* Included after the project's native Windows boundary. The caller supplies a
 * provenance-verified, complete ASCII save-directory path. No directory is
 * created and no caller-selected filename is accepted. Calls are serialized.
 */
#define SS_MAX_BYTES 8192u
#define SS_ROOT_MAX 180u
#define SS_PATH_MAX 260u
#define SS_MAX_DIRS 64u
#define SS_FILE_NAME "MoonLab.satellite-gps.v1.bin"
#ifdef _WIN32
#define SS_CALL __stdcall
#else
#define SS_CALL
#endif

enum {
  SS_NONE = 0,
  SS_BAD_PATH = 1,
  SS_BAD_SIZE = 2,
  SS_NOT_FOUND = 3,
  SS_REPARSE = 4,
  SS_IO = 5,
  SS_BUSY = 6,
  SS_UNBOUND = 7
};
enum {
  SS_STAGE_NONE = 0,
  SS_STAGE_OPEN_DIRECTORY = 1,
  SS_STAGE_INFO = 2,
  SS_STAGE_OPEN_FILE = 3,
  SS_STAGE_SIZE = 4,
  SS_STAGE_READ = 5,
  SS_STAGE_CREATE_TEMP = 6,
  SS_STAGE_WRITE = 7,
  SS_STAGE_FLUSH = 8,
  SS_STAGE_CLOSE = 9,
  SS_STAGE_MOVE = 10,
  SS_STAGE_REMOVE = 11
};
typedef struct {
  DWORD attributes, times[6], volume, size_high, size_low, links, index_high,
      index_low;
} SSFileInfo;
typedef char SS_file_info_layout[(sizeof(SSFileInfo) == 52) ? 1 : -1];
typedef struct {
  char root[SS_ROOT_MAX + 1], file[SS_PATH_MAX], temp[SS_PATH_MAX];
  PTR directories[SS_MAX_DIRS];
  unsigned int count;
} SSPaths;
static struct {
  BOOL(SS_CALL *read)(PTR, PTR, DWORD, DWORD *, PTR);
  DWORD(SS_CALL *size)(PTR, DWORD *);
  BOOL(SS_CALL *move)(const char *, const char *, DWORD);
  BOOL(SS_CALL *close)(PTR);
  BOOL(SS_CALL *info)(PTR, SSFileInfo *);
  BOOL(SS_CALL *remove)(const char *);
  DWORD(SS_CALL *error)(void);
  unsigned int ready, busy, last_error, serial, native_stage;
  DWORD native_error;
  unsigned char scratch[SS_MAX_BYTES];
} ss;
static inline void ss_zero(void *dst, U64 length) {
  unsigned char *p = dst;
  while (length--)
    *p++ = 0;
}
static inline void ss_copy(void *dst, const void *src, U64 length) {
  unsigned char *d = dst;
  const unsigned char *s = src;
  while (length--)
    *d++ = *s++;
}
static inline int ss_handle(PTR h) { return h && h != (PTR)(U64)-1; }
static inline char ss_upper(char c) {
  return c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
}
static inline int ss_device(const char *s, unsigned int n) {
  unsigned int base = 0;
  while (base < n && s[base] != '.')
    base++;
  if (base == 3) {
    char a = ss_upper(s[0]), b = ss_upper(s[1]), c = ss_upper(s[2]);
    if ((a == 'C' && b == 'O' && c == 'N') ||
        (a == 'P' && b == 'R' && c == 'N') ||
        (a == 'A' && b == 'U' && c == 'X') ||
        (a == 'N' && b == 'U' && c == 'L'))
      return 1;
  }
  if (base == 4 && s[3] >= '1' && s[3] <= '9') {
    char a = ss_upper(s[0]), b = ss_upper(s[1]), c = ss_upper(s[2]);
    if ((a == 'C' && b == 'O' && c == 'M') ||
        (a == 'L' && b == 'P' && c == 'T'))
      return 1;
  }
  return 0;
}
static inline int ss_root_path(char *out, const char *root) {
  unsigned int n = 0, start;
  if (!root)
    return 0;
  while (n <= SS_ROOT_MAX && root[n]) {
    unsigned char ch = (unsigned char)root[n];
    if (ch < 32 || ch >= 127)
      return 0;
    out[n] = ch == '/' ? '\\' : (char)ch;
    n++;
  }
  if (n > SS_ROOT_MAX || n < 4 || out[1] != ':' || out[2] != '\\' ||
      ss_upper(out[0]) < 'A' || ss_upper(out[0]) > 'Z')
    return 0;
  if (out[n - 1] == '\\')
    n--;
  out[n] = 0;
  start = 3;
  for (unsigned int i = 3; i <= n; i++) {
    unsigned char ch = (unsigned char)out[i];
    if (ch == '\\' || !ch) {
      unsigned int length = i - start;
      if (!length || out[i - 1] == '.' || out[i - 1] == ' ' ||
          ss_device(out + start, length))
        return 0;
      start = i + 1;
    } else if (ch == ':' || ch == '<' || ch == '>' || ch == '"' || ch == '|' ||
               ch == '?' || ch == '*')
      return 0;
  }
  return 1;
}
static inline int ss_append(char *out, unsigned int cap, const char *text) {
  unsigned int n = 0, i = 0;
  while (n < cap && out[n])
    n++;
  if (n == cap)
    return 0;
  while (text[i]) {
    if (n + 1 >= cap)
      return 0;
    out[n++] = text[i++];
  }
  out[n] = 0;
  return 1;
}
static inline int ss_number(char *out, unsigned int cap, DWORD number) {
  char digits[10], text[11];
  unsigned int n = 0, i;
  do {
    digits[n++] = (char)('0' + number % 10);
    number /= 10;
  } while (number);
  for (i = 0; i < n; i++)
    text[i] = digits[n - 1 - i];
  text[n] = 0;
  return ss_append(out, cap, text);
}
static inline int ss_bind(void) {
  if (ss.ready)
    return 1;
  PTR kernel = LoadLibraryA("kernel32.dll");
  if (!kernel)
    return 0;
#define SS_BIND(F, N, T)                                                       \
  ss.F = (T)GetProcAddress(kernel, N);                                         \
  if (!ss.F)                                                                   \
  return 0
  SS_BIND(read, "ReadFile", BOOL(SS_CALL *)(PTR, PTR, DWORD, DWORD *, PTR));
  SS_BIND(size, "GetFileSize", DWORD(SS_CALL *)(PTR, DWORD *));
  SS_BIND(move, "MoveFileExA",
          BOOL(SS_CALL *)(const char *, const char *, DWORD));
  SS_BIND(close, "CloseHandle", BOOL(SS_CALL *)(PTR));
  SS_BIND(info, "GetFileInformationByHandle",
          BOOL(SS_CALL *)(PTR, SSFileInfo *));
  SS_BIND(remove, "DeleteFileA", BOOL(SS_CALL *)(const char *));
  SS_BIND(error, "GetLastError", DWORD(SS_CALL *)(void));
#undef SS_BIND
  ss.ready = 1;
  return 1;
}
/* Preserve the first failing native operation before cleanup changes the
 * thread's GetLastError value. A logical validation error has no native stage.
 */
static inline void ss_note_failure(unsigned int stage, DWORD error) {
  if (!ss.native_stage) {
    ss.native_stage = stage;
    ss.native_error = error;
  }
  ss.last_error = SS_IO;
}
static inline int ss_close_file(PTR *handle) {
  int result = 1;
  if (ss_handle(*handle)) {
    result = ss.close(*handle) != 0;
    if (!result)
      ss_note_failure(SS_STAGE_CLOSE, ss.error());
  }
  if (result)
    *handle = 0;
  return result;
}
static inline int ss_close_directories(SSPaths *p) {
  int ok = 1;
  while (p->count) {
    PTR *handle = &p->directories[--p->count];
    if (!ss_close_file(handle)) {
      ok = 0;
      ss_close_file(handle);
    }
  }
  return ok;
}
static inline int ss_directory(SSPaths *p, const char *name) {
  if (p->count >= SS_MAX_DIRS) {
    ss.last_error = SS_BAD_PATH;
    return 0;
  }
  /* No share-write/delete: pin each ancestor against modification or rename
   * while paths are used. OPEN_REPARSE_POINT prevents following a junction. */
  PTR h = CreateFileA(name, 0x80, 1, 0, 3, 0x02200000, 0);
  SSFileInfo info;
  ss_zero(&info, sizeof(info));
  if (!ss_handle(h)) {
    ss_note_failure(SS_STAGE_OPEN_DIRECTORY, ss.error());
    return 0;
  }
  int info_ok = ss.info(h, &info) != 0;
  if (!info_ok)
    ss_note_failure(SS_STAGE_INFO, ss.error());
  if (!info_ok || !(info.attributes & 0x10) || (info.attributes & 0x400)) {
    ss.last_error = (info.attributes & 0x400) ? SS_REPARSE : SS_IO;
    if (!ss_close_file(&h))
      ss_close_file(&h);
    return 0;
  }
  p->directories[p->count++] = h;
  return 1;
}
static inline int ss_paths(SSPaths *p, const char *root) {
  ss_zero(p, sizeof(*p));
  if (!ss_root_path(p->root, root)) {
    ss.last_error = SS_BAD_PATH;
    return 0;
  }
  char saved = p->root[3];
  p->root[3] = 0;
  int ok = ss_directory(p, p->root);
  p->root[3] = saved;
  if (!ok)
    return 0;
  for (unsigned int i = 3;; i++)
    if (!p->root[i] || p->root[i] == '\\') {
      char ch = p->root[i];
      p->root[i] = 0;
      ok = ss_directory(p, p->root);
      p->root[i] = ch;
      if (!ok)
        return 0;
      if (!ch)
        break;
    }
  if (!ss_append(p->file, SS_PATH_MAX, p->root) ||
      !ss_append(p->file, SS_PATH_MAX, "\\" SS_FILE_NAME)) {
    ss.last_error = SS_BAD_PATH;
    return 0;
  }
  return 1;
}
static inline int ss_file_info(PTR handle) {
  SSFileInfo info;
  ss_zero(&info, sizeof(info));
  if (!ss.info(handle, &info)) {
    ss_note_failure(SS_STAGE_INFO, ss.error());
    return 0;
  }
  if (info.attributes & 0x400) {
    ss.last_error = SS_REPARSE;
    return 0;
  }
  if (info.attributes & 0x10) {
    ss.last_error = SS_IO;
    return 0;
  }
  return 1;
}
static inline int ss_begin(void) {
  if (!ss.ready) {
    ss.last_error = SS_UNBOUND;
    return 0;
  }
  if (ss.busy) {
    ss.last_error = SS_BUSY;
    return 0;
  }
  ss.busy = 1;
  ss.last_error = SS_NONE;
  ss.native_error = 0;
  ss.native_stage = SS_STAGE_NONE;
  return 1;
}
static inline int ss_load(const char *root, unsigned char *bytes,
                          unsigned int capacity, unsigned int *size) {
  SSPaths paths;
  PTR handle = 0;
  int ok = 0;
  DWORD high = 0, length = 0, read = 0;
  if (size)
    *size = 0;
  if (!bytes || !size || !capacity) {
    ss.last_error = SS_BAD_SIZE;
    return 0;
  }
  if (!ss_begin())
    return 0;
  if (!ss_paths(&paths, root))
    goto done;
  handle = CreateFileA(paths.file, 0x80000000, 1, 0, 3, 0x00200000, 0);
  if (!ss_handle(handle)) {
    DWORD error = ss.error();
    ss_note_failure(SS_STAGE_OPEN_FILE, error);
    if (error == 2)
      ss.last_error = SS_NOT_FOUND;
    handle = 0;
    goto done;
  }
  if (!ss_file_info(handle))
    goto done;
  length = ss.size(handle, &high);
  if (high || !length || length > SS_MAX_BYTES || length > capacity) {
    ss.last_error = SS_BAD_SIZE;
    goto done;
  }
  while (read < length) {
    DWORD got = 0, want = length - read;
    int read_ok = ss.read(handle, ss.scratch + read, want, &got, 0) != 0;
    if (!read_ok || !got || got > want) {
      ss_note_failure(SS_STAGE_READ, read_ok ? 0 : ss.error());
      goto done;
    }
    read += got;
  }
  if (!ss_close_file(&handle)) {
    ss.last_error = SS_IO;
    goto done;
  }
  ok = 1;
done:
  {
    int file_closed = ss_close_file(&handle);
    if (!file_closed)
      ss_close_file(&handle);
    int dirs_closed = ss_close_directories(&paths);
    if (!file_closed || !dirs_closed) {
      ss.last_error = SS_IO;
      ok = 0;
    }
  }
  if (ok) {
    ss_copy(bytes, ss.scratch, length);
    *size = (unsigned int)length;
  }
  ss.busy = 0;
  return ok;
}
static inline int ss_save(const char *root, const unsigned char *bytes,
                          unsigned int size) {
  SSPaths paths;
  PTR handle = 0, existing = 0;
  int ok = 0, temp_created = 0;
  DWORD written = 0;
  if (!bytes || !size || size > SS_MAX_BYTES) {
    ss.last_error = SS_BAD_SIZE;
    return 0;
  }
  if (!ss_begin())
    return 0;
  if (!ss_paths(&paths, root))
    goto done;
  /* Open without traversing a link, inspect its type, and keep it read-only.
   * Close this handle before rename: Soda rejects replacement of an open
   * destination with ACCESS_DENIED even when it shares READ|WRITE|DELETE. */
  existing = CreateFileA(paths.file, 0x80, 5, 0, 3, 0x00200000, 0);
  if (ss_handle(existing)) {
    if (!ss_file_info(existing))
      goto done;
  } else {
    existing = 0;
    DWORD error = ss.error();
    if (error != 2) {
      ss_note_failure(SS_STAGE_OPEN_FILE, error);
      goto done;
    }
  }
  if (ss.serial == 0xffffffffu) {
    ss.last_error = SS_IO;
    goto done;
  }
  ss.serial++;
  if (!ss_append(paths.temp, SS_PATH_MAX, paths.file) ||
      !ss_append(paths.temp, SS_PATH_MAX, ".tmp.") ||
      !ss_number(paths.temp, SS_PATH_MAX, GetCurrentProcessId()) ||
      !ss_append(paths.temp, SS_PATH_MAX, ".") ||
      !ss_number(paths.temp, SS_PATH_MAX, ss.serial)) {
    ss.last_error = SS_BAD_PATH;
    goto done;
  }
  handle = CreateFileA(paths.temp, 0x40000080, 0, 0, 1, 0x00200080, 0);
  if (!ss_handle(handle)) {
    handle = 0;
    ss_note_failure(SS_STAGE_CREATE_TEMP, ss.error());
    goto done;
  }
  temp_created = 1;
  if (!ss_file_info(handle))
    goto done;
  while (written < size) {
    DWORD got = 0, want = (DWORD)size - written;
    int write_ok = WriteFile(handle, bytes + written, want, &got, 0) != 0;
    if (!write_ok || !got || got > want) {
      ss_note_failure(SS_STAGE_WRITE, write_ok ? 0 : ss.error());
      goto done;
    }
    written += got;
  }
  if (!FlushFileBuffers(handle)) {
    ss_note_failure(SS_STAGE_FLUSH, ss.error());
    goto done;
  }
  if (!ss_close_file(&handle))
    goto done;
  if (!ss_close_file(&existing))
    goto done;
  if (!ss.move(paths.temp, paths.file, 9)) {
    ss_note_failure(SS_STAGE_MOVE, ss.error());
    goto done;
  }
  temp_created = 0;
  ok = 1;
done:
  {
    int file_closed = ss_close_file(&handle);
    if (!file_closed)
      ss_close_file(&handle);
    int existing_closed = ss_close_file(&existing);
    if (!existing_closed)
      ss_close_file(&existing);
    if (!file_closed || !existing_closed) {
      ss.last_error = SS_IO;
      ok = 0;
    }
  }
  if (temp_created && !ss.remove(paths.temp))
    ss_note_failure(SS_STAGE_REMOVE, ss.error());
  if (!ss_close_directories(&paths)) {
    ss.last_error = SS_IO;
    ok = 0;
  }
  ss.busy = 0;
  return ok;
}
#undef SS_CALL
#endif
