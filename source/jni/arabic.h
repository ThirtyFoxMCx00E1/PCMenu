// PCMenu - Arabic text support (included by main.cpp before the font atlas code)
//
//  The game has no Arabic font and no Arabic text file, so the PC menu does it all itself:
//   * strings are plain UTF-8 everywhere in the mod ("logical order", the order the text is written and read in),
//   * only when a string with Arabic letters is MEASURED or DRAWN it goes through ArShape(): every letter is replaced by the
//     glyph of its joined form (isolated / initial / medial / final, lam-alef ligatures) and the text is put in visual
//     (left to right) order: Arabic runs reversed, Latin letters, digits and numbers kept in reading order (a simplified
//     Unicode bidirectional algorithm for a right-to-left paragraph - menu texts are one short line).
//  The glyph codes it returns are the Unicode presentation forms U+FE80..U+FEFC (plus the Arabic comma / semicolon / question
//  mark U+060C, U+061B, U+061F) for the Arabic atlases bank_ar / beck_ar made by tools/gen_fonts.py, and plain ASCII for the rest.
#ifndef PCARABIC_H
#define PCARABIC_H

struct ArLetter { uint16_t base, iso, fin, ini, med; };      // 0 = the form does not exist (letters that never join to the next one)
static const ArLetter kArLetters[] = {      // generated from the Unicode decomposition data (<isolated>/<final>/<initial>/<medial>)
    { 0x0621, 0xFE80, 0x0000, 0x0000, 0x0000 }, { 0x0622, 0xFE81, 0xFE82, 0x0000, 0x0000 }, { 0x0623, 0xFE83, 0xFE84, 0x0000, 0x0000 },
    { 0x0624, 0xFE85, 0xFE86, 0x0000, 0x0000 }, { 0x0625, 0xFE87, 0xFE88, 0x0000, 0x0000 }, { 0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C },
    { 0x0627, 0xFE8D, 0xFE8E, 0x0000, 0x0000 }, { 0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92 }, { 0x0629, 0xFE93, 0xFE94, 0x0000, 0x0000 },
    { 0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98 }, { 0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C }, { 0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0 },
    { 0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4 }, { 0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8 }, { 0x062F, 0xFEA9, 0xFEAA, 0x0000, 0x0000 },
    { 0x0630, 0xFEAB, 0xFEAC, 0x0000, 0x0000 }, { 0x0631, 0xFEAD, 0xFEAE, 0x0000, 0x0000 }, { 0x0632, 0xFEAF, 0xFEB0, 0x0000, 0x0000 },
    { 0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4 }, { 0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8 }, { 0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC },
    { 0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0 }, { 0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4 }, { 0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8 },
    { 0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC }, { 0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0 }, { 0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4 },
    { 0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8 }, { 0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC }, { 0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0 },
    { 0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4 }, { 0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8 }, { 0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC },
    { 0x0648, 0xFEED, 0xFEEE, 0x0000, 0x0000 }, { 0x0649, 0xFEEF, 0xFEF0, 0x0000, 0x0000 }, { 0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4 },
};
static const int kArLetterCount = (int)(sizeof(kArLetters) / sizeof(kArLetters[0]));
struct ArLig { uint16_t alef, iso, fin; };                    // lam + alef variant
static const ArLig kArLigs[] = { { 0x0622, 0xFEF5, 0xFEF6 }, { 0x0623, 0xFEF7, 0xFEF8 }, { 0x0625, 0xFEF9, 0xFEFA }, { 0x0627, 0xFEFB, 0xFEFC } };

#define AR_GLYPHS 128                                         // atlas slots: U+FE80..U+FEFC (125), then U+060C, U+061B, U+061F
static int ArSlot(unsigned code)
{
    if(code >= 0xFE80 && code <= 0xFEFC) return (int)(code - 0xFE80);
    if(code == 0x060C) return 125;
    if(code == 0x061B) return 126;
    if(code == 0x061F) return 127;
    return -1;
}

/* true when the text holds Arabic letters (UTF-8 lead bytes D8..DB = U+0600..U+06FF) */
static bool HasArabic(const std::string& s)
{
    for(size_t i = 0; i < s.size(); ++i) { unsigned char c = (unsigned char)s[i]; if(c >= 0xD8 && c <= 0xDB) return true; }
    return false;
}

static void Utf8ToCps(const std::string& s, std::vector<uint32_t>& cps)
{
    for(size_t i = 0; i < s.size();)
    {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp; size_t n;
        if(c < 0x80) { cps.push_back(c); ++i; continue; }
        else if((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
        else if((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
        else if((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 4; }
        else { cps.push_back('?'); ++i; continue; }
        bool ok = (i + n <= s.size());
        for(size_t k = 1; ok && k < n; ++k) { unsigned char d = (unsigned char)s[i + k]; if((d & 0xC0) != 0x80) ok = false; else cp = (cp << 6) | (d & 0x3F); }
        if(!ok) { cps.push_back('?'); ++i; continue; }
        cps.push_back(cp); i += n;
    }
}
static std::string CpToUtf8(uint32_t cp)
{
    std::string r;
    if(cp < 0x80) r += (char)cp;
    else if(cp < 0x800) { r += (char)(0xC0 | (cp >> 6)); r += (char)(0x80 | (cp & 0x3F)); }
    else { r += (char)(0xE0 | (cp >> 12)); r += (char)(0x80 | ((cp >> 6) & 0x3F)); r += (char)(0x80 | (cp & 0x3F)); }
    return r;
}
/* removes the last whole character (never half of a UTF-8 sequence) */
static void Utf8Pop(std::string& s)
{
    if(s.empty()) return;
    size_t n = s.size() - 1;
    while(n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) --n;
    s.erase(n);
}

static const ArLetter* ArLetterOf(uint32_t cp)
{
    if(cp < 0x0621 || cp > 0x064A) return NULL;
    for(int i = 0; i < kArLetterCount; ++i) if(kArLetters[i].base == cp) return &kArLetters[i];
    return NULL;
}
static bool ArIsMark(uint32_t cp) { return (cp >= 0x064B && cp <= 0x065F) || cp == 0x0670 || cp == 0x0640 || cp == 0x200C || cp == 0x200D || cp == 0x200E || cp == 0x200F; }   // vowel signs, tatweel, joiners: not drawn

struct ArItem { uint16_t code; uint8_t dir; uint8_t kind; };   // dir: 0 = left-to-right, 1 = right-to-left, 2 = neutral (resolved below); kind: 1 digit, 2 number separator, 3 percent
static bool ArIsDigit(uint32_t c) { return (c >= '0' && c <= '9') || (c >= 0x0660 && c <= 0x0669); }
static bool ArIsBracket(unsigned c, unsigned* mirror)
{
    switch(c) { case '(': *mirror = ')'; return true; case ')': *mirror = '('; return true; case '[': *mirror = ']'; return true; case ']': *mirror = '['; return true;
                case '<': *mirror = '>'; return true; case '>': *mirror = '<'; return true; case '{': *mirror = '}'; return true; case '}': *mirror = '{'; return true; }
    return false;
}

/* Visual order glyph codes of one line of text (base direction right-to-left). */
static void ArShape(const std::string& text, std::vector<uint16_t>& out)
{
    out.clear();
    std::vector<uint32_t> raw, cps;
    Utf8ToCps(text, raw);
    for(size_t i = 0; i < raw.size(); ++i) if(!ArIsMark(raw[i])) cps.push_back(raw[i]);
    const size_t n = cps.size();
    std::vector<ArItem> items; items.reserve(n);

    for(size_t i = 0; i < n; ++i)
    {
        uint32_t c = cps[i];
        const ArLetter* L = ArLetterOf(c);
        if(L)
        {
            const ArLetter* P = (i > 0) ? ArLetterOf(cps[i - 1]) : NULL;
            const ArLetter* N = (i + 1 < n) ? ArLetterOf(cps[i + 1]) : NULL;
            bool joinPrev = P && P->ini != 0;                        // the letter before connects to the left (= to us)
            if(c == 0x0644 && N)                                     // lam + alef: one ligature glyph, connected to the letter before the lam when that one joins
            {
                const ArLig* lig = NULL;
                for(size_t k = 0; k < sizeof(kArLigs) / sizeof(kArLigs[0]); ++k) if(kArLigs[k].alef == N->base) lig = &kArLigs[k];
                if(lig) { ArItem it = { joinPrev ? lig->fin : lig->iso, 1, 0 }; items.push_back(it); ++i; continue; }
            }
            bool joinNext = (L->ini != 0) && N && N->fin != 0;       // we connect to the left and the next letter takes a connection
            if(joinPrev && L->fin == 0) joinPrev = false;
            uint16_t code = (joinPrev && joinNext) ? L->med : joinPrev ? L->fin : joinNext ? L->ini : L->iso;
            if(!code) code = L->iso;
            ArItem it = { code, 1, 0 }; items.push_back(it);
            continue;
        }
        ArItem it = { (uint16_t)'?', 2, 0 };
        if(ArIsDigit(c)) { it.code = (uint16_t)(c >= 0x0660 ? '0' + (c - 0x0660) : c); it.dir = 0; it.kind = 1; }
        else if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) { it.code = (uint16_t)c; it.dir = 0; }
        else if(c == 0x060C || c == 0x061B || c == 0x061F) { it.code = (uint16_t)c; it.dir = 2; }
        else if(c >= 32 && c < 127) { it.code = (uint16_t)c; it.dir = 2; it.kind = (c == '.' || c == ',' || c == ':' || c == '/') ? 2 : (c == '%' ? 3 : 0); }
        else { it.code = '?'; it.dir = 2; }
        items.push_back(it);
    }

    // numbers: 12.5  1,000  10:30  50%  stay together as one left-to-right run
    for(size_t i = 0; i < items.size(); ++i)
    {
        if(items[i].kind == 2 && i > 0 && i + 1 < items.size() && items[i - 1].kind == 1 && items[i + 1].kind == 1) items[i].dir = 0;
        else if(items[i].kind == 3 && i > 0 && items[i - 1].kind == 1) items[i].dir = 0;
    }
    // neutrals take the direction of the text around them when both sides agree, the paragraph direction (right-to-left) otherwise
    for(size_t i = 0; i < items.size();)
    {
        if(items[i].dir != 2) { ++i; continue; }
        size_t j = i; while(j < items.size() && items[j].dir == 2) ++j;
        uint8_t before = (i > 0) ? items[i - 1].dir : 1, after = (j < items.size()) ? items[j].dir : 1;
        uint8_t d = (before == after) ? before : 1;
        for(size_t k = i; k < j; ++k) items[k].dir = d;
        i = j;
    }
    // mirrored brackets inside right-to-left runs
    for(size_t i = 0; i < items.size(); ++i) { unsigned m; if(items[i].dir == 1 && items[i].code < 128 && ArIsBracket(items[i].code, &m)) items[i].code = (uint16_t)m; }

    // visual order: the runs in reverse order, the right-to-left ones also reversed inside
    size_t end = items.size();
    while(end > 0)
    {
        size_t start = end; while(start > 0 && items[start - 1].dir == items[end - 1].dir) --start;
        if(items[end - 1].dir == 1) for(size_t k = end; k > start; --k) out.push_back(items[k - 1].code);
        else                        for(size_t k = start; k < end; ++k) out.push_back(items[k].code);
        end = start;
    }
}

#endif
