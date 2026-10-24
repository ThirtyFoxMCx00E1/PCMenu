/* GXT reader: loads a text file of the game's own format (INDONESIAN.GXT) so the Indonesian PC menu can also give Indonesian
   subtitles / HUD texts / menu texts. Header-only, no dependencies on the game, tested on the host (hosttest/test.cpp).

   Format (what libGTASA's CText::Load / LoadMissionText read, verified in the 2.00 and 2.10 libraries):
     [u16 version][u16 bits per char]      (4 byte header, the game skips it without looking; PC files have 8 bits, Android files 16)
     "TABL" [u32 size]  size/12 entries: char name[8], u32 offset            <- table of contents ("MAIN", then one per mission)
     MAIN table (right after TABL, at its offset):     "TKEY" [u32 size] size/8 entries (u32 offset, u32 hash)   "TDAT" [u32 size] chars
     other tables (at their offset): char name[8] then the same TKEY / TDAT
     hash = CKeyGen::GetUppercaseKey(key): CRC-32 of the upper-cased key, initial value ~0, no final inversion; entries sorted by hash
     offsets are byte offsets into the TDAT data; the game's chars are 16 bit (8 bit PC files are widened here), 0 terminated.
   The game looks a key up in MAIN first, then in the loaded mission table: Find() does the same. */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>

namespace gxt {

static inline uint32_t KeyHash(const char* s)
{
    struct Tab { uint32_t t[256]; Tab() { for(uint32_t i = 0; i < 256; ++i) { uint32_t c = i; for(int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1); t[i] = c; } } };
    static const Tab tab;                                   // (thread safe initialisation since C++11)
    uint32_t h = 0xFFFFFFFFu;
    for(; *s; ++s) { unsigned char c = (unsigned char)*s; if(c >= 'a' && c <= 'z') c -= 32; h = tab.t[(h ^ c) & 0xFF] ^ (h >> 8); }
    return h;
}

struct Entry { uint32_t hash; uint32_t idx; };               // idx = first char of the text in Table::text
struct Table
{
    char name[9];
    std::vector<Entry> keys;                                 // sorted by hash, no duplicates
    std::vector<uint16_t> text;                              // 16 bit chars, always ends with a 0
    Table() { memset(name, 0, sizeof(name)); }
    const uint16_t* Find(uint32_t h) const
    {
        size_t lo = 0, hi = keys.size();
        while(lo < hi) { size_t mid = (lo + hi) / 2; if(keys[mid].hash < h) lo = mid + 1; else hi = mid; }
        return (lo < keys.size() && keys[lo].hash == h) ? &text[keys[lo].idx] : NULL;
    }
};

class Db
{
public:
    std::vector<Table> tabs;                                 // [0] = MAIN
    int bits = 0;                                            // 8 or 16 (as read from the header)
    int version = 0;
    size_t keyCount = 0, badTables = 0, badKeys = 0;

    void Clear() { tabs.clear(); bits = version = 0; keyCount = badTables = badKeys = 0; }

    static uint32_t U32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
    static uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

    /* true when MAIN was read (mission tables that are damaged are skipped and counted in badTables) */
    bool Parse(const uint8_t* d, size_t n, std::string* err = NULL)
    {
        Clear();
        if(n < 16) { if(err) *err = "file too small"; return false; }
        size_t pos = 0;
        if(memcmp(d, "TABL", 4) == 0) { bits = 16; version = 0; }                       // no header (not a real game file, accepted)
        else
        {
            version = U16(d); bits = U16(d + 2); pos = 4;
            if(bits != 8 && bits != 16) bits = 16;                                      // the game ignores the header and always reads 16 bit chars
        }
        if(pos + 8 > n || memcmp(d + pos, "TABL", 4) != 0) { if(err) *err = "no TABL block (not a GXT file)"; return false; }
        uint32_t tsize = U32(d + pos + 4);
        size_t tstart = pos + 8;
        if(tsize > n - tstart) { if(err) *err = "TABL block runs past the end of the file"; return false; }
        size_t entries = tsize / 12;
        if(entries == 0) { if(err) *err = "empty TABL block"; return false; }

        struct TE { char name[9]; uint32_t off; };
        std::vector<TE> te;
        for(size_t i = 0; i < entries; ++i)
        {
            const uint8_t* e = d + tstart + i * 12; TE t; memcpy(t.name, e, 8); t.name[8] = 0; t.off = U32(e + 8);
            for(int k = 0; k < 8; ++k) if(t.name[k] == 0) { for(int j = k; j < 8; ++j) t.name[j] = 0; break; }
            te.push_back(t);
        }
        size_t afterTabl = tstart + tsize;

        // MAIN: the entry named MAIN, else what follows TABL
        size_t mainAt = afterTabl;
        for(size_t i = 0; i < te.size(); ++i) if(strcmp(te[i].name, "MAIN") == 0) { mainAt = te[i].off; break; }
        Table mainT; strcpy(mainT.name, "MAIN");
        if(!ParseTable(d, n, mainAt, mainT, true))
        {
            if(mainAt != afterTabl) { mainT = Table(); strcpy(mainT.name, "MAIN"); if(!ParseTable(d, n, afterTabl, mainT, true)) { if(err) *err = "MAIN table unreadable"; return false; } }
            else { if(err) *err = "MAIN table unreadable"; return false; }
        }
        tabs.push_back(mainT);
        for(size_t i = 0; i < te.size(); ++i)
        {
            if(strcmp(te[i].name, "MAIN") == 0 || !te[i].name[0]) continue;
            bool dup = false; for(size_t k = 1; k < tabs.size(); ++k) if(strcmp(tabs[k].name, te[i].name) == 0) dup = true;
            if(dup) continue;
            Table t; memcpy(t.name, te[i].name, 9);
            if(ParseTable(d, n, te[i].off, t, false)) tabs.push_back(t); else ++badTables;
        }
        keyCount = 0; for(size_t k = 0; k < tabs.size(); ++k) keyCount += tabs[k].keys.size();
        if(tabs[0].keys.empty()) { if(err) *err = "MAIN table has no texts"; return false; }
        return true;
    }

    bool LoadFile(const std::string& path, std::string* err = NULL)
    {
        FILE* f = fopen(path.c_str(), "rb");
        if(!f) { if(err) *err = "cannot open"; return false; }
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if(sz <= 0 || sz > 48L * 1024 * 1024) { fclose(f); if(err) *err = sz <= 0 ? "empty file" : "file is larger than 48 MB"; return false; }
        std::vector<uint8_t> buf((size_t)sz);
        size_t got = fread(buf.data(), 1, buf.size(), f); fclose(f);
        if(got != buf.size()) { if(err) *err = "read error"; return false; }
        return Parse(buf.data(), buf.size(), err);
    }

    const Table* TableByName(const char* name) const
    {
        if(!name || !name[0]) return NULL;
        for(size_t i = 1; i < tabs.size(); ++i) if(strncmp(tabs[i].name, name, 8) == 0 && strlen(name) == strlen(tabs[i].name)) return &tabs[i];
        return NULL;
    }
    /* MAIN first, then the loaded mission table: the same order as CText::Get. NULL when the file has no such text. */
    const uint16_t* Find(const char* key, const char* mission = NULL) const
    {
        if(tabs.empty() || !key || !key[0]) return NULL;
        uint32_t h = KeyHash(key);
        const uint16_t* t = tabs[0].Find(h);
        if(t) return t;
        const Table* m = TableByName(mission);
        return m ? m->Find(h) : NULL;
    }
    /* any table (for tests / lookups that do not know the mission) */
    const uint16_t* FindAnywhere(const char* key) const
    {
        if(tabs.empty() || !key || !key[0]) return NULL;
        uint32_t h = KeyHash(key);
        for(size_t i = 0; i < tabs.size(); ++i) { const uint16_t* t = tabs[i].Find(h); if(t) return t; }
        return NULL;
    }

private:
    /* One table starting at `at`: [8 byte name when it is not MAIN] "TKEY" ... "TDAT" ... (any other block in between is skipped, as the game does) */
    bool ParseTable(const uint8_t* d, size_t n, size_t at, Table& t, bool isMain)
    {
        if(at > n || n - at < 8) return false;
        if(memcmp(d + at, "TKEY", 4) != 0 && memcmp(d + at, "TDAT", 4) != 0)
        {
            if(isMain && memcmp(d + at, "TABL", 4) == 0) return false;
            at += 8;                                                                      // the table's own name
        }
        const uint8_t *keyP = NULL, *datP = NULL; size_t keyN = 0, datN = 0;
        for(int guard = 0; guard < 24 && at + 8 <= n; ++guard)
        {
            uint32_t sz = U32(d + at + 4); const uint8_t* body = d + at + 8;
            if(sz > n - (at + 8)) return false;
            if(memcmp(d + at, "TKEY", 4) == 0 && !keyP) { keyP = body; keyN = sz; }
            else if(memcmp(d + at, "TDAT", 4) == 0 && !datP) { datP = body; datN = sz; }
            else if(memcmp(d + at, "TABL", 4) == 0) return false;                         // ran into the table of contents: this table has no texts
            at += 8 + sz;                                                                 // (any other block is skipped, like the game does)
            if(keyP && datP) break;
        }
        if(!keyP || !datP) return false;

        size_t chars = bits == 8 ? datN : datN / 2;
        t.text.assign(chars + 1, 0);
        if(bits == 8) for(size_t i = 0; i < chars; ++i) t.text[i] = datP[i];
        else          for(size_t i = 0; i < chars; ++i) t.text[i] = U16(datP + i * 2);
        size_t cnt = keyN / 8;
        t.keys.reserve(cnt);
        for(size_t i = 0; i < cnt; ++i)
        {
            uint32_t off = U32(keyP + i * 8), h = U32(keyP + i * 8 + 4);
            size_t idx = bits == 8 ? off : off / 2;
            if((bits != 8 && (off & 1)) || idx >= chars) { ++badKeys; continue; }
            Entry e; e.hash = h; e.idx = (uint32_t)idx; t.keys.push_back(e);
        }
        std::stable_sort(t.keys.begin(), t.keys.end(), [](const Entry& a, const Entry& b) { return a.hash < b.hash; });
        size_t w = 0;                                                                     // duplicates: the first one in the file wins
        for(size_t i = 0; i < t.keys.size(); ++i) if(w == 0 || t.keys[w - 1].hash != t.keys[i].hash) t.keys[w++] = t.keys[i];
        t.keys.resize(w);
        return true;
    }
};

}   // namespace gxt
