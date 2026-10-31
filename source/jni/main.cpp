// PCMenu - PC style main/pause menu for GTA San Andreas (Android)
//
// Builds as libPCMenu32.so (armeabi-v7a, game v2.00) and libPCMenu64.so (arm64-v8a, game v2.10).
// Needs AndroidModLoader only. Textures live in .../Android/data/com.rockstargames.gtasa/files/PCMenu/
//
// How it works (no game structs are needed beyond a few verified offsets):
//   * MobileMenu::Render  -> after the stock menu has rendered, we paint the PC menu on top of it.
//   * MobileMenu::Update  -> while the PC menu is up, the stock menu is told "no finger is down"
//                            (we zero the game's `pointers` touch table around the stock call), so its hidden buttons
//                            can never fire. Our own touch handling runs right after Update.
//   * Menu entries call the *real* callbacks stored in the stock FlowScreen item list
//     (Resume / Options / Stats / Brief / Start Game / Quit), so game behaviour is unchanged.
#include <mod/amlmod.h>
#include <mod/logger.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>
#include <vector>
#include <string>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <set>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

/* Logging goes to the AML logger (logcat) only. The mod no longer creates any log file in files/. */
#define LOGI(...) logger->Info(__VA_ARGS__)
#define LOGE(...) logger->Error(__VA_ARGS__)

#ifdef PCMENU_TRANSPARENT
MYMOD(net.gtasa.pcmenu, PC Menu (Transparent), 1.2.1, PCMenu)
#else
MYMOD(net.gtasa.pcmenu, PC Menu, 1.2.1, PCMenu)
#endif
NEEDGAME(com.rockstargames.gtasa)

/* ------------------------------------------------------------------------------------------ */
/*  Verified struct offsets (see README: derived from the exported MobileMenu / MainMenuScreen   */
/*  code of libGTASA 2.00 armeabi-v7a and 2.10 arm64-v8a)                                        */
/* ------------------------------------------------------------------------------------------ */
#ifdef AML32
    #define OFF_MM_SCREENCOUNT   0x24   // MobileMenu: int   screen count
    #define OFF_MM_SCREENS       0x28   // MobileMenu: MenuScreen** screen stack
    #define OFF_MM_PENDING       0x2C   // MobileMenu: screen to push / return to (set by OnBack & co, handled later by ProcessPending)
    #define OFF_MM_MAPMODE       0x6D   // MobileMenu: bool  full-screen "map mode" (stock map scrolling) - see EnforceMapMode
    #define OFF_SCR_ITEMCOUNT    0x40   // FlowScreen: int   item count
    #define OFF_SCR_ITEMS        0x44   // FlowScreen: Item* items
    #define OFF_SCR_ISPAUSE      0x0D   // FlowScreen: bool  is pause screen
    #define OFF_RS_WIDTH         0x04   // RsGlobal.maximumWidth
    #define OFF_RS_HEIGHT        0x08   // RsGlobal.maximumHeight
    #define PTR_STRIDE           0x70   // sizeof(pointers[0])
    #define GOT_RENDER           0x674254   // .got.plt slot of MobileMenu::Render
    #define GOT_UPDATE           0x66E754   // .got.plt slot of MobileMenu::Update
    #define GOT_ESCAPE           0x66EAA4   // .got.plt slot of CPad::GetEscapeJustDown
    #define FP_RENDER 0x299B65
    #define FP_UPDATE 0x29A6C1
    #define FP_ESCAPE 0x3FCBA5
    #define FP_FLOWRENDER 0x29C031
    #define FP_VTABLE 0x6625D0
    #define PTR_SZ 4
    #define SEL_ITEMCOUNT 0x18   // SelectScreen: int count
    #define SEL_ITEMS     0x1C   // SelectScreen: MenuSelection** items
    #define ITEM_ONSELECT 0x08   // ActionSelection::OnSelect
    #define ITEM_ACTDATA  0x0C   // ActionSelection::data
    #define ITEM_SETID    0x08   // SettingSelection::forSetting
    #define SET_STRIDE 0x20      // sizeof(MobileSettings::Setting)
    #define SET_NAMES  0x04
    #define SET_VALUE  0x08
    #define SET_MIN    0x10
    #define SET_MAX    0x14
    #define SET_TYPE   0x1C
    #define GOT_SETTINGS 0x679A40   // GOT slot of MobileSettings::settings (SAUtils redirects it)
    #define OFF_SCR_CHOSEN 0x30     // FlowScreen: int m_nChosenButton (the tile the stock UI is activating)
    #define GOT_BACKPRESSED 0x66EB3C
    #define FP_BACKPRESSED  0x2972B5
    #define GOT_TITLESAFE   0x672BC0   // .got.plt slot of CWidget::DrawTitleSafeArea: the very last call of MobileMenu::Render
    #define FP_TITLESAFE    0x2B409F
    #define GOT_REPORTFE    0x6703FC   // .got.plt slot of CAudioEngine::ReportFrontendAudioEvent(int, float, float): every menu click of the game goes through it
    #define FP_REPORTFE     0x3BCA9D
    #define GOT_TEXTGET     0x66E78C   // .got.plt slot of CText::Get(const char*): every text lookup of the game (subtitles, HUD, menus) outside CText itself
    #define FP_TEXTGET      0x54DB8D
    #define OFF_MSG   0x44          // YesNoScreen / OkScreen: message GXT key (const char*), read by ConfirmationScreen::Render
    #define MM_PSTATE 0x90          // gMobileMenu: per-device pointer state[] (ActionSelection::HandleInput only acts when state[lastDevice] == 0)
#else
    #define OFF_MM_SCREENCOUNT   0x24
    #define OFF_MM_SCREENS       0x28
    #define OFF_MM_PENDING       0x30
    #define OFF_MM_MAPMODE       0x8D
    #define OFF_SCR_ITEMCOUNT    0x5C
    #define OFF_SCR_ITEMS        0x60
    #define OFF_SCR_ISPAUSE      0x15
    #define OFF_RS_WIDTH         0x08
    #define OFF_RS_HEIGHT        0x0C
    #define PTR_STRIDE           0x78
    #define GOT_RENDER           0x846FE8
    #define GOT_UPDATE           0x83DC08
    #define GOT_ESCAPE           0x83E130
    #define FP_RENDER 0x355DFC
    #define FP_UPDATE 0x356A7C
    #define FP_ESCAPE 0x4E038C
    #define FP_FLOWRENDER 0x358794
    #define FP_VTABLE 0x825AA0
    #define PTR_SZ 8
    #define SEL_ITEMCOUNT 0x24
    #define SEL_ITEMS     0x28
    #define ITEM_ONSELECT 0x10
    #define ITEM_ACTDATA  0x18
    #define ITEM_SETID    0x10
    #define SET_STRIDE 0x28
    #define SET_NAMES  0x08
    #define SET_VALUE  0x10
    #define SET_MIN    0x18
    #define SET_MAX    0x1C
    #define SET_TYPE   0x24
    #define GOT_SETTINGS 0x851498
    #define OFF_SCR_CHOSEN 0x48
    #define GOT_BACKPRESSED 0x83E238
    #define FP_BACKPRESSED  0x3532D0
    #define GOT_TITLESAFE   0x844B28
    #define FP_TITLESAFE    0x37350C
    #define GOT_REPORTFE    0x840A78
    #define FP_REPORTFE     0x49AADC
    #define GOT_TEXTGET     0x83DC48
    #define FP_TEXTGET      0x66DB9C
    #define OFF_MSG   0x60
    #define MM_PSTATE 0xB0
#endif

struct StockItem            // one entry of the stock FlowScreen button list (12 bytes on 32-bit, 24 on 64-bit)
{
    void*       tex;
    const char* key;        // GXT key, e.g. "FEP_RES"
    void      (*cb)();      // called with no arguments by FlowScreen::Update
};

struct V2  { float x, y; };
struct Col { uint8_t r, g, b, a; };

typedef void (*DrawSpriteFn)(void* tex, V2* verts, V2* uvs, Col* cols);
typedef void (*DrawQuadFn)(V2* verts, Col* cols);
typedef void (*RenderStateSetFn)(int state, void* value);
#define PTR_X 0x0C
#define PTR_Y 0x10
typedef int  (*PtrButtonFn)(int ptr, int btn);
typedef int  (*PtrCoordsFn)(int ptr, int* x, int* y, float* pressure);
typedef bool (*IsPauseFn)(void* screen);
typedef void (*VoidFn)();
typedef void*(*PngReadFn)(const char*);
typedef void*(*FindRasterFmtFn)(void* img, int type, int* w, int* h, int* d, int* flags);
typedef void*(*RasterCreateFn)(int w, int h, int d, int flags);
typedef void*(*RasterFromImgFn)(void* raster, void* img);
typedef void*(*TexCreateFn)(void* raster);
typedef void (*ImgDestroyFn)(void* img);
typedef void (*ProcessPendingFn)(void* mm);

/* RenderWare render states used */
enum { RS_TEXRASTER = 1, RS_TEXADDRESS = 2, RS_ZTEST = 6, RS_ZWRITE = 8, RS_TEXFILTER = 9,
       RS_SRCBLEND = 10, RS_DESTBLEND = 11, RS_VERTEXALPHA = 12 };

/* ------------------------------------------------------------------------------------------ */
/*  Globals                                                                                     */
/* ------------------------------------------------------------------------------------------ */
static void*      g_hGame = NULL;
static uintptr_t  g_vtMainMenu = 0;
static int*       g_pRsGlobal = NULL;
static void**     g_ppAdjHUD  = NULL;    // CAdjustableHUD::m_pInstance: non-null while the stock "Change button position" editor is up
static bool       g_editorUp  = false;   // result of the last EvalTakeover(): the stock editor owns the screen
static uint8_t*   g_pGangWars = NULL;    // CGangWars::bGangWarsActive: the game draws the map's gang territory only while the story has switched gang wars on
static uint16_t*  g_pNavZones = NULL;    // CTheZones::TotalNumberOfNavigationZones (0 = nothing to draw either)

static DrawSpriteFn     fnDrawSprite   = NULL;
static DrawQuadFn       fnDrawQuad     = NULL;
static RenderStateSetFn fnRenderState  = NULL;
static PtrCoordsFn      fnPtrCoords    = NULL;
static PtrButtonFn      fnPtrButton    = NULL;
static bool           (*fnIsPlaying)(bool) = NULL;   // Menu_IsPlayingGame: true while a game world exists
static uint8_t*         g_pointers     = NULL;   // exported `pointers` array: 4 x { int state[3]; ... int x, y; ... }
static uintptr_t        g_libBase      = 0;
static IsPauseFn        fnIsPause      = NULL;
static ProcessPendingFn fnProcessPending = NULL;
static PngReadFn        fnPngRead      = NULL;
static FindRasterFmtFn  fnFindRasterFmt = NULL;
static RasterCreateFn   fnRasterCreate = NULL;
static RasterFromImgFn  fnRasterFromImg = NULL;
static TexCreateFn      fnTexCreate    = NULL;
static ImgDestroyFn     fnImgDestroy   = NULL;
static VoidFn fnOnResume = NULL, fnOnStartGame = NULL, fnOnStats = NULL,
              fnOnBriefs = NULL, fnOnSettings = NULL, fnOnExit = NULL;

static std::string g_dirPC;        // .../files/PCMenu/
static std::string g_dirMods;      // .../mods/

/* config (PCMenu/pcmenu.ini) */
static bool  g_cfgEnabled     = true;
static bool  g_cfgModsInMain  = false;   // ShowModsInMenu=1 brings the MODS entry back (the Mods list now lives in Options)
static bool  g_cfgSounds      = true;
static bool  g_cfgPCOptions   = true;
static float g_cfgScale       = 1.0f;
static std::string g_cfgBackground = "back8.png";
static bool  g_cfgIdGxt       = true;    // IndonesianText=0 turns off the use of INDONESIAN.GXT (game texts stay in the game's language)
static bool  g_cfgIdGxtLog    = true;    // IndonesianTextLog=0: do not write idgxt_missing.txt (the texts the file lacks)
static bool  g_cfgArMirror    = true;    // ArabicMirror=0 keeps the PC layout (labels left, values right) in Arabic too; 1 mirrors it (right to left)
static int   g_cfgLang        = 0;       // Language= in pcmenu.ini (also set from the Language page): 0 = follow the game, LG_ID = Indonesian, LG_AR = Arabic (the game itself has neither text file)
static bool  g_arOK           = false;   // the Arabic glyph atlases (bank_ar / beck_ar) were found: only then Arabic can be chosen

static StockItem* g_inertItems; static void* g_inertTop; 
static bool g_suppressFlow = false, g_backRequested = false, g_touchLogged = false;
static bool g_paintPending = false; static int g_paintKind = 0; static void* g_paintTop = NULL;   // the PC menu is painted once per frame, at the end of the stock pass
static void* g_suppressTop = NULL;
static bool g_resTried = false, g_resOK = false, g_layoutLogged = false, g_badLayoutLogged = false;

/* ------------------------------------------------------------------------------------------ */
/*  Small helpers                                                                               */
/* ------------------------------------------------------------------------------------------ */
static std::string Upper(const std::string& s)
{
    std::string r = s;
    for(size_t i = 0; i < r.size(); ++i) if(r[i] >= 'a' && r[i] <= 'z') r[i] -= 32;
    return r;
}

static bool FileExists(const std::string& p) { return access(p.c_str(), R_OK) == 0; }

/* Reads memory without crashing if the pointer is bogus (write() to a pipe returns EFAULT) */
static int g_pipe[2] = { -1, -1 };
static bool SafeCopy(const void* src, void* dst, size_t n)
{
    if(!src || n == 0 || n > 256) return false;
    if(g_pipe[0] < 0 && pipe2(g_pipe, O_NONBLOCK | O_CLOEXEC) != 0) return false;
    ssize_t w = write(g_pipe[1], src, n);
    if(w <= 0) return false;
    uint8_t tmp[256];
    ssize_t r = read(g_pipe[0], w == (ssize_t)n ? dst : tmp, (size_t)w);
    return (w == (ssize_t)n) && (r == w);
}
static bool SafeCString(const char* p, char* out, size_t cap)
{
    // copy byte by byte only on cache rebuild (not per frame)
    for(size_t i = 0; i + 1 < cap; ++i)
    {
        char c;
        if(!SafeCopy(p + i, &c, 1)) return false;
        out[i] = c;
        if(c == 0) return true;
        if((unsigned char)c < 32 || (unsigned char)c > 126) return false;
    }
    out[cap - 1] = 0;
    return true;
}

#include "arabic.h"
#include "pclang.h"
#include "gxt.h"

static std::string ReadFileText(const std::string& path)
{
    std::string s; FILE* f = fopen(path.c_str(), "rb"); if(!f) return s;
    char buf[1024]; size_t n; while((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f); return s;
}

static void SaveLanguageSetting()                         // rewrites (or adds) only the Language= line of pcmenu.ini
{
    std::string path = g_dirPC + "pcmenu.ini", txt = ReadFileText(path), out;
    std::string line = std::string("Language=") + (g_cfgLang == LG_ID ? "Indonesian" : g_cfgLang == LG_AR ? "Arabic" : "Auto");
    bool done = false; size_t pos = 0;
    while(pos < txt.size())
    {
        size_t e = txt.find('\n', pos); bool last = (e == std::string::npos); if(last) e = txt.size();
        std::string l = txt.substr(pos, e - pos); pos = e + 1;
        size_t b = l.find_first_not_of(" \t");
        if(b != std::string::npos && l.compare(b, 8, "Language") == 0 && l.find('=') != std::string::npos && l.find_first_not_of(" \t", b + 8) == l.find('=')) { l = line; done = true; }
        out += l; if(!last) out += '\n';
    }
    if(!done) { if(!out.empty() && out.back() != '\n') out += '\n'; out += line + "\n"; }
    FILE* f = fopen(path.c_str(), "wb");
    if(!f) { LOGE("Cannot write %s (the language choice is kept until the game closes)", path.c_str()); return; }
    fwrite(out.data(), 1, out.size(), f); fclose(f);
    LOGI("Language saved to pcmenu.ini: %s", line.c_str());
}

static void ExtraConfig(const std::string& k, const std::string& v);   // pcextra.h
static bool TransparentBackdrop();                                       // pcextra.h
static bool TransparentPicture();
static void LoadResources();
static std::string OwnerOf(uintptr_t addr);
static void LoadConfig()
{
    std::string txt = ReadFileText(g_dirPC + "pcmenu.ini");
    if(txt.empty()) return;
    size_t pos = 0;
    while(pos < txt.size())
    {
        size_t e = txt.find('\n', pos); if(e == std::string::npos) e = txt.size();
        std::string line = txt.substr(pos, e - pos); pos = e + 1;
        while(!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if(line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;
        size_t eq = line.find('='); if(eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        while(!k.empty() && k.back() == ' ') k.pop_back();
        while(!v.empty() && v[0] == ' ') v.erase(0, 1);
        if(k == "Enabled")            g_cfgEnabled = atoi(v.c_str()) != 0;
        else if(k == "ShowModsInMenu") g_cfgModsInMain = atoi(v.c_str()) != 0;
        else if(k == "Sounds")        g_cfgSounds = atoi(v.c_str()) != 0;
        else if(k == "PCOptions")     g_cfgPCOptions = atoi(v.c_str()) != 0;
        else if(k == "Scale")         { g_cfgScale = (float)atof(v.c_str()); if(g_cfgScale < 0.5f || g_cfgScale > 2.0f) g_cfgScale = 1.0f; }
        else if(k == "Background")    g_cfgBackground = v;
        else if(k == "ArabicMirror")  g_cfgArMirror = atoi(v.c_str()) != 0;
        else if(k == "IndonesianText")    g_cfgIdGxt = atoi(v.c_str()) != 0;
        else if(k == "IndonesianTextLog") g_cfgIdGxtLog = atoi(v.c_str()) != 0;
        else if(k == "LangMissingLog")    g_cfgLangLog = atoi(v.c_str()) != 0;
        else if(k == "Language")
        {
            std::string u = Upper(v);
            g_cfgLang = (u == "INDONESIAN" || u == "INDONESIA" || u == "ID" || u == "BAHASA INDONESIA") ? (int)LG_ID
                      : (u == "ARABIC" || u == "AR" || u == "ARABIA" || u == "\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A\xD8\xA9") ? (int)LG_AR : 0;
        }
        else ExtraConfig(k, v);
    }
}

/* ------------------------------------------------------------------------------------------ */
/*  Textures + font atlases                                                                     */
/* ------------------------------------------------------------------------------------------ */
static void* LoadPNG(const std::string& rel)
{
    std::string abs = g_dirPC + rel;
    void* img = FileExists(abs) ? fnPngRead(abs.c_str()) : NULL;
    if(!img) img = fnPngRead(("PCMenu/" + rel).c_str());   // relative to the game's files dir as a fallback
    if(!img) { LOGE("PNG not readable: %s", abs.c_str()); return NULL; }
    void* tex = NULL;
    int w, h, d, flags;
    fnFindRasterFmt(img, 4 /*rwRASTERTYPETEXTURE*/, &w, &h, &d, &flags);
    if(void* raster = fnRasterCreate(w, h, d, flags))
    {
        fnRasterFromImg(raster, img);
        tex = fnTexCreate(raster);
    }
    fnImgDestroy(img);
    return tex;
}

struct Glyph { float u0, v0, u1, v1; int w, h, xoff, yoff; float adv; bool valid; };
struct FontAtlas
{
    void* tex; int capH; float spaceEx, xs, bold; Glyph g[128];
    void* texAr; Glyph ar[AR_GLYPHS]; bool hasAr;           // Arabic companion atlas (<name>_ar): presentation forms, see arabic.h
    FontAtlas() : tex(NULL), capH(30), spaceEx(0), xs(1), bold(0), texAr(NULL), hasAr(false) { memset(g, 0, sizeof(g)); memset(ar, 0, sizeof(ar)); }

    bool LoadArabic(const char* base)                        // optional: without it the PC menu simply has no Arabic
    {
        std::string txt = ReadFileText(g_dirPC + "pc/fonts/" + base + ".txt");
        if(txt.empty()) return false;
        int size, asc, desc, cap, aw, ah;
        if(sscanf(txt.c_str(), "FONT %d %d %d %d %d %d", &size, &asc, &desc, &cap, &aw, &ah) != 6 || aw <= 0 || ah <= 0) return false;
        size_t pos = txt.find('\n'); int n = 0;
        while(pos != std::string::npos && pos < txt.size())
        {
            ++pos;
            int code, x, y, w, h, xo, yo; float adv;
            if(sscanf(txt.c_str() + pos, "G %d %d %d %d %d %d %d %f", &code, &x, &y, &w, &h, &xo, &yo, &adv) == 8 && ArSlot((unsigned)code) >= 0)
            {
                Glyph& gl = ar[ArSlot((unsigned)code)];
                gl.u0 = (float)x / aw; gl.v0 = (float)y / ah; gl.u1 = (float)(x + w) / aw; gl.v1 = (float)(y + h) / ah;
                gl.w = w; gl.h = h; gl.xoff = xo; gl.yoff = yo; gl.adv = adv; gl.valid = true; ++n;
            }
            pos = txt.find('\n', pos);
        }
        if(n < 100) return false;                           // not the atlas tools/gen_fonts.py makes
        texAr = LoadPNG(std::string("pc/fonts/") + base + ".png");
        hasAr = (texAr != NULL);
        return hasAr;
    }
    const Glyph& GetAr(unsigned code) const { int i = ArSlot(code); if(i < 0 || !ar[i].valid) return g['?']; return ar[i]; }   // (a missing form is measured, and drawn, as the Latin '?')

    bool Load(const char* base)
    {
        std::string txt = ReadFileText(g_dirPC + "pc/fonts/" + base + ".txt");
        if(txt.empty()) { LOGE("font description missing: %spc/fonts/%s.txt", g_dirPC.c_str(), base); return false; }
        int size, asc, desc, cap, aw, ah;
        if(sscanf(txt.c_str(), "FONT %d %d %d %d %d %d", &size, &asc, &desc, &cap, &aw, &ah) != 6) return false;
        capH = cap;
        size_t pos = txt.find('\n');
        while(pos != std::string::npos && pos < txt.size())
        {
            ++pos;
            int code, x, y, w, h, xo, yo; float adv;
            if(sscanf(txt.c_str() + pos, "G %d %d %d %d %d %d %d %f", &code, &x, &y, &w, &h, &xo, &yo, &adv) == 8 && code >= 32 && code < 128)
            {
                Glyph& gl = g[code];
                gl.u0 = (float)x / aw; gl.v0 = (float)y / ah; gl.u1 = (float)(x + w) / aw; gl.v1 = (float)(y + h) / ah;
                gl.w = w; gl.h = h; gl.xoff = xo; gl.yoff = yo; gl.adv = adv; gl.valid = true;
            }
            pos = txt.find('\n', pos);
        }
        tex = LoadPNG(std::string("pc/fonts/") + base + ".png");
        return tex != NULL;
    }
    const Glyph& Get(unsigned char c) const { if(c < 32 || c > 126 || !g[c].valid) c = '?'; return g[c]; }
    /* advance of one glyph of a shaped (Arabic) line: Arabic letters touch each other (no letter spacing), Latin glyphs inside keep it */
    float Advance(unsigned code, float scale, float track, bool last) const
    {
        if(code == ' ') return GetAr(0xFE8D).adv * scale;      // a space between Arabic words: as wide as an alef (about a third of the em), not the wide PC-font space
        if(code < 128) return Get((unsigned char)code).adv * scale * xs + (code == ' ' ? spaceEx : 0) + (last ? 0 : track);
        return GetAr(code).adv * scale;
    }
    float Width(const std::string& s, float scale, float track) const
    {
        float w = 0;
        if(hasAr && HasArabic(s))
        {
            std::vector<uint16_t> sh; ArShape(s, sh);
            for(size_t i = 0; i < sh.size(); ++i) w += Advance(sh[i], scale, track, i + 1 == sh.size());
            return w;
        }
        for(size_t i = 0; i < s.size(); ++i) w += Get((unsigned char)s[i]).adv * scale * xs + (s[i] == ' ' ? spaceEx : 0) + (i + 1 < s.size() ? track : 0);
        return w;
    }
};
static FontAtlas g_bank, g_beck;
static void *g_texArrow = NULL, *g_texBack = NULL;

/* ------------------------------------------------------------------------------------------ */
/*  Drawing primitives (all coordinates are screen pixels)                                      */
/* ------------------------------------------------------------------------------------------ */
static const Col COL_GOLD  = { 171, 131, 12, 255 };
static const Col COL_DIM   = {  66,  57, 10, 255 };
static const Col COL_BLACK = {   0,   0,  0, 255 };
static const Col COL_WHITE = { 255, 255, 255, 255 };

static void SetStates()
{
    fnRenderState(RS_ZTEST, (void*)0);
    fnRenderState(RS_ZWRITE, (void*)0);
    fnRenderState(RS_VERTEXALPHA, (void*)1);
    fnRenderState(RS_SRCBLEND, (void*)5);   // rwBLENDSRCALPHA
    fnRenderState(RS_DESTBLEND, (void*)6);  // rwBLENDINVSRCALPHA
    fnRenderState(RS_TEXFILTER, (void*)2);  // rwFILTERLINEAR
    fnRenderState(RS_TEXADDRESS, (void*)3); // rwTEXTUREADDRESSCLAMP
}

static void FillRect(float x0, float y0, float x1, float y1, Col c)
{
    V2 v[4] = { {x0, y0}, {x1, y0}, {x0, y1}, {x1, y1} };
    Col cc[4] = { c, c, c, c };
    fnDrawQuad(v, cc);
}
static void DrawTex(void* tex, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Col c)
{
    V2 v[4]  = { {x0, y0}, {x1, y0}, {x0, y1}, {x1, y1} };
    V2 uv[4] = { {u0, v0}, {u1, v0}, {u0, v1}, {u1, v1} };
    Col cc[4] = { c, c, c, c };
    fnDrawSprite(tex, v, uv, cc);
}
static void DrawText(const FontAtlas& f, const std::string& s, float x, float baseline, float scale, float track, Col c)
{
    float pen = x;
    if(f.hasAr && HasArabic(s))                              // Arabic: joined letter forms in visual order (arabic.h), from the companion atlas
    {
        std::vector<uint16_t> sh; ArShape(s, sh);
        for(size_t i = 0; i < sh.size(); ++i)
        {
            unsigned code = sh[i];
            if(code < 128)                                   // Latin letter / digit / space between Arabic words: the PC font
            {
                const Glyph& g = f.Get((unsigned char)code);
                if(g.w > 0 && g.h > 0)
                {
                    float gx = pen + g.xoff * scale * f.xs, gy = baseline + g.yoff * scale, gx1 = gx + g.w * scale * f.xs, gy1 = gy + g.h * scale;
                    DrawTex(f.tex, gx, gy, gx1, gy1, g.u0, g.v0, g.u1, g.v1, c);
                    if(f.bold > 0) DrawTex(f.tex, gx + f.bold, gy, gx1 + f.bold, gy1, g.u0, g.v0, g.u1, g.v1, c);
                }
            }
            else
            {
                int si = ArSlot(code);
                if(si >= 0 && f.ar[si].valid)
                {
                    const Glyph& g = f.ar[si];
                    if(g.w > 0 && g.h > 0) DrawTex(f.texAr, pen + g.xoff * scale, baseline + g.yoff * scale, pen + (g.xoff + g.w) * scale, baseline + (g.yoff + g.h) * scale, g.u0, g.v0, g.u1, g.v1, c);
                }
                else { const Glyph& q = f.Get('?'); if(q.w > 0 && q.h > 0) DrawTex(f.tex, pen + q.xoff * scale * f.xs, baseline + q.yoff * scale, pen + (q.xoff + q.w) * scale * f.xs, baseline + (q.yoff + q.h) * scale, q.u0, q.v0, q.u1, q.v1, c); }
            }
            pen += f.Advance(code, scale, track, i + 1 == sh.size());
        }
        return;
    }
    for(size_t i = 0; i < s.size(); ++i)
    {
        const Glyph& g = f.Get((unsigned char)s[i]);
        if(g.w > 0 && g.h > 0)
        {
            float gx = pen + g.xoff * scale * f.xs, gy = baseline + g.yoff * scale;
            float gx1 = gx + g.w * scale * f.xs, gy1 = gy + g.h * scale;
            DrawTex(f.tex, gx, gy, gx1, gy1, g.u0, g.v0, g.u1, g.v1, c);
            if(f.bold > 0) DrawTex(f.tex, gx + f.bold, gy, gx1 + f.bold, gy1, g.u0, g.v0, g.u1, g.v1, c);   // synthetic bold
        }
        pen += g.adv * scale * f.xs + track + (s[i] == ' ' ? f.spaceEx : 0);
    }
}
static std::string Fit(const FontAtlas& f, std::string s, float scale, float track, float maxW)
{
    if(f.Width(s, scale, track) <= maxW) return s;
    while(s.size() > 1 && f.Width(s + "...", scale, track) > maxW) Utf8Pop(s);
    return s + "...";
}

/* ------------------------------------------------------------------------------------------ */
/*  Sounds (OpenSL ES)                                                                          */
/* ------------------------------------------------------------------------------------------ */
struct Snd
{
    std::vector<uint8_t> pcm; SLObjectItf obj; SLPlayItf play; SLAndroidSimpleBufferQueueItf bq; bool ok;
    Snd() : obj(NULL), play(NULL), bq(NULL), ok(false) {}
};
static SLObjectItf g_slEngineObj = NULL, g_slMixObj = NULL;
static SLEngineItf g_slEngine = NULL;
static Snd g_sndSelect, g_sndBack;
static bool g_slTried = false;

static bool SndInit()
{
    if(g_slTried) return g_slEngine != NULL;
    g_slTried = true;
    if(slCreateEngine(&g_slEngineObj, 0, NULL, 0, NULL, NULL) != SL_RESULT_SUCCESS) return false;
    if((*g_slEngineObj)->Realize(g_slEngineObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) return false;
    if((*g_slEngineObj)->GetInterface(g_slEngineObj, SL_IID_ENGINE, &g_slEngine) != SL_RESULT_SUCCESS) { g_slEngine = NULL; return false; }
    if((*g_slEngine)->CreateOutputMix(g_slEngine, &g_slMixObj, 0, NULL, NULL) != SL_RESULT_SUCCESS) { g_slEngine = NULL; return false; }
    if((*g_slMixObj)->Realize(g_slMixObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) { g_slEngine = NULL; return false; }
    return true;
}
static bool SndLoad(Snd& s, const std::string& path)
{
    if(!SndInit()) return false;
    std::string d = ReadFileText(path);
    if(d.size() < 44 || memcmp(d.data(), "RIFF", 4) || memcmp(d.data() + 8, "WAVE", 4)) return false;
    int ch = 0, rate = 0, bits = 0; size_t dataOff = 0, dataLen = 0;
    for(size_t p = 12; p + 8 <= d.size();)
    {
        uint32_t len; memcpy(&len, d.data() + p + 4, 4);
        if(!memcmp(d.data() + p, "fmt ", 4) && p + 8 + 16 <= d.size())
        {
            uint16_t fmt, c, b; uint32_t sr;
            memcpy(&fmt, d.data() + p + 8, 2); memcpy(&c, d.data() + p + 10, 2);
            memcpy(&sr, d.data() + p + 12, 4); memcpy(&b, d.data() + p + 22, 2);
            if(fmt != 1) return false;
            ch = c; rate = (int)sr; bits = b;
        }
        else if(!memcmp(d.data() + p, "data", 4)) { dataOff = p + 8; dataLen = std::min<size_t>(len, d.size() - dataOff); break; }
        p += 8 + len + (len & 1);
    }
    if(!dataLen || bits != 16 || (ch != 1 && ch != 2)) return false;
    s.pcm.assign((const uint8_t*)d.data() + dataOff, (const uint8_t*)d.data() + dataOff + dataLen);

    SLDataLocator_AndroidSimpleBufferQueue locBq = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2 };
    SLDataFormat_PCM pcm = { SL_DATAFORMAT_PCM, (SLuint32)ch, (SLuint32)rate * 1000, SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                             ch == 1 ? (SLuint32)SL_SPEAKER_FRONT_CENTER : (SLuint32)(SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT), SL_BYTEORDER_LITTLEENDIAN };
    SLDataSource src = { &locBq, &pcm };
    SLDataLocator_OutputMix locOut = { SL_DATALOCATOR_OUTPUTMIX, g_slMixObj };
    SLDataSink snk = { &locOut, NULL };
    const SLInterfaceID ids[1] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
    const SLboolean req[1] = { SL_BOOLEAN_TRUE };
    if((*g_slEngine)->CreateAudioPlayer(g_slEngine, &s.obj, &src, &snk, 1, ids, req) != SL_RESULT_SUCCESS) return false;
    if((*s.obj)->Realize(s.obj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) return false;
    if((*s.obj)->GetInterface(s.obj, SL_IID_PLAY, &s.play) != SL_RESULT_SUCCESS) return false;
    if((*s.obj)->GetInterface(s.obj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &s.bq) != SL_RESULT_SUCCESS) return false;
    if((*s.play)->SetPlayState(s.play, SL_PLAYSTATE_PLAYING) != SL_RESULT_SUCCESS) return false;
    s.ok = true;
    return true;
}
/* One tap = one click. Every code path that wants a sound goes through SndPlay, and a second request within CLICK_GAP_MS of the
   last one is dropped (so a clip that is still playing is never cut and restarted, which was audible as a pop / double click). */
static int64_t (*g_clockFn)() = NULL;                                    // (the host test drives its own clock)
static int64_t NowMs()
{
    if(g_clockFn) return g_clockFn();
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000);
}
static const int64_t CLICK_GAP_MS = 150;
static int64_t g_sndLastMs = -100000;
static int g_sndPlayed = 0, g_sndGated = 0;
static int g_feSwallowed = 0;                                            // the last game click (frontend event 1 / 2) that the hook below kept from playing
static void SndPlay(Snd& s)
{
    if(!g_cfgSounds || !s.ok) return;
    int64_t now = NowMs();
    if(now - g_sndLastMs < CLICK_GAP_MS) { ++g_sndGated; return; }
    g_sndLastMs = now; ++g_sndPlayed;
    (*s.bq)->Clear(s.bq);
    (*s.bq)->Enqueue(s.bq, s.pcm.data(), (SLuint32)s.pcm.size());
}

/* ------------------------------------------------------------------------------------------ */
/*  Menu model                                                                                  */
/* ------------------------------------------------------------------------------------------ */
enum eView { VIEW_MAIN = 0, VIEW_MODS, VIEW_MAP };
enum eKind { K_STOCK = 0, K_MAP, K_MODS };
enum eScr  { SK_NONE = 0, SK_ROOT, SK_OPTIONS, SK_PAGE, SK_START };   // which kind of stock screen the PC menu currently replaces (SK_START = Start Game: New / Load / Delete tiles)
enum ePageStyle { PS_SETTINGS = 0, PS_SLOTS, PS_DIALOG, PS_TILES_CENTER, PS_TILES_LEFT };   // how the current page is laid out
enum eModRow { MR_FILE = 0, MR_TAB, MR_SETTINGS };
enum eNav  { NAV_NONE = 0, NAV_WAIT_OPTIONS, NAV_WAIT_PAGE };

struct Entry { std::string label; int kind; const char* key; VoidFn fallback; };
struct ModRow { std::string label, file; VoidFn cb; int kind; int idx; };

struct Theme { Col title, dim, sel; };
static const Theme TH_BLUE = { { 172, 203, 241, 255 }, {  74,  90, 107, 255 }, { 172, 203, 241, 255 } };   // title / selected / unselected: flat pixel values sampled from the real PC "Main Menu" screenshot

/* one row of a stock SelectScreen settings page (Game / Controls / Audio / Display / Language / Mods settings) */
struct PRow
{
    int kind;                       // 0 = button (ActionSelection), 1 = option / toggle, 2 = slider, 3 = save slot (Load / Save / Delete screens), 4 = a language only the PC menu knows (Bahasa Indonesia, Arabic; added by this mod to the Language page, data = language)
    void* item; int setId;
    std::string label;
    void (*act)(void*, int); int data;
    std::string valText; int lastVal;
};
struct OptRow { std::string label, title; VoidFn cb; int idx; };       // one tile of the stock Options screen (SettingsScreen)

static int   g_view = VIEW_MAIN;
static int   g_kind = SK_NONE;
static int   g_sel = 0;                 // selected row; -2 = the BACK button
static float g_scroll = 0;
static std::vector<Entry>  g_entries;
static std::vector<ModRow> g_modRows;
static std::vector<PRow>   g_pageRows;
static std::vector<OptRow> g_optRows;
static void* g_pageRowsTop = NULL; static void* g_pageRowsArr = NULL; static int g_pageRowsCount = -1;
static unsigned g_optRowsGen = 0xFFFFFFFFu;
static std::string g_pageTitle = "Options";
static int   g_pageBg = -1;             // picture of the open options page (-1: choose it from the title)
static int   g_stockCount = 0;
static StockItem g_stock[32];
static char  g_stockKeys[32][40];
static void* g_cacheTop = NULL; static void* g_cacheItems = NULL; static int g_cacheCount = -1; static int g_cacheAge = 0;
static bool  g_isPause = false;
static bool  g_inStockUpdate = false;
static int64_t g_liveMs = -100000;                                       // when the PC menu was last updated / drawn (g_kind alone goes stale while the menu is closed)
static void* g_activeTop = NULL; static uintptr_t g_activeVt = 0;

/* navigation helper: Mods view -> Options -> "Mods settings" page, and back */
static int  g_nav = NAV_NONE, g_navFrames = 0;
static bool g_returnToMods = false, g_autoBack = false;
static std::string g_navFilter;
static int  g_pendingRootView = -1; static int g_modsSelSaved = 0; static float g_modsScrollSaved = 0;
static std::string g_toast; static int g_toastFrames = 0;

/* text / settings access (resolved in OnModLoad) */
typedef unsigned short* (*TextGetFn)(void* self, const char* key);
static TextGetFn fnTextGet = NULL;
static void* g_pTheText = NULL;
static unsigned short* g_gxtMissing = NULL; static bool g_gxtMissingInit = false;

/* ---- Indonesian game texts: INDONESIAN.GXT ------------------------------------------------------------------------------------
   The game has no Indonesian text file, so subtitles / HUD / menu texts stay in the game's language. When the PC menu language is
   Indonesian and the user supplied INDONESIAN.GXT (the game's own GXT format, see gxt.h) in files/PCMenu/, CText::Get is hooked (GOT
   slot, no code patch): a key that the file has is answered from the file, everything else from the game as before. The mission
   table is chosen with CText::GetNameOfLoadedMissionText, the same table the game itself searches second. */
typedef void (*MissionNameFn)(void* self, char* out);
static TextGetFn      TextGet_orig = NULL;                       // CText::Get as it was before our hook (the game's, or another mod's hook)
static MissionNameFn  fnMissionName = NULL;
static gxt::Db*       g_idDb = NULL;                             // never freed: the game keeps pointers into the texts
static std::atomic<int> g_idState(0);                            // 0 not tried yet, 1 loading, 2 ready, 3 no usable file
static std::mutex     g_idMutex;
static std::string    g_idFile;

static std::string LowerStr(const std::string& s) { std::string r = s; for(size_t i = 0; i < r.size(); ++i) if(r[i] >= 'A' && r[i] <= 'Z') r[i] += 32; return r; }
static std::string FindIdGxtFile()
{
    static const char* const names[] = { "indonesian.gxt", "indonesia.gxt", "bahasa.gxt", "id.gxt" };
    std::string parent = g_dirPC;                                // .../files/PCMenu/ -> .../files/ is searched second (by name only)
    if(parent.size() > 7 && parent.compare(parent.size() - 7, 7, "PCMenu/") == 0) parent.erase(parent.size() - 7);
    const std::string dirs[2] = { g_dirPC, parent };
    for(int di = 0; di < 2; ++di)
    {
        if(di == 1 && (parent.empty() || parent == g_dirPC)) break;
        std::vector<std::string> found;
        if(DIR* d = opendir(dirs[di].c_str()))
        {
            while(dirent* e = readdir(d)) { std::string n = e->d_name, l = LowerStr(n); if(l.size() > 4 && l.compare(l.size() - 4, 4, ".gxt") == 0) found.push_back(n); }
            closedir(d);
        }
        for(size_t k = 0; k < sizeof(names) / sizeof(names[0]); ++k)
            for(size_t i = 0; i < found.size(); ++i) if(LowerStr(found[i]) == names[k]) return dirs[di] + found[i];
        if(di == 0 && found.size() == 1) return dirs[0] + found[0];            // a single .gxt in PCMenu/ is taken as the Indonesian file whatever it is called
    }
    return "";
}
static bool IdGxtEnsure()                                        // true when the Indonesian text file is loaded (loads it the first time)
{
    int s = g_idState.load();
    if(s == 2) return true;
    if(s != 0) return false;
    std::lock_guard<std::mutex> lk(g_idMutex);
    s = g_idState.load();
    if(s == 2) return true;
    if(s != 0) return false;
    g_idState = 1;
    g_idFile = FindIdGxtFile();
    if(g_idFile.empty()) { LOGI("Indonesian texts: no INDONESIAN.GXT in %s - the game's texts stay in the game's language", g_dirPC.c_str()); g_idState = 3; return false; }
    gxt::Db* db = new gxt::Db; std::string err;
    if(!db->LoadFile(g_idFile, &err)) { LOGE("Indonesian texts: %s is not usable (%s) - the game's texts stay in the game's language", g_idFile.c_str(), err.c_str()); delete db; g_idState = 3; return false; }
    LOGI("Indonesian texts: %s loaded - %d tables, %d texts, %d-bit characters%s", g_idFile.c_str(), (int)db->tabs.size(), (int)db->keyCount, db->bits,
         (db->badTables || db->badKeys) ? " (some damaged entries were skipped)" : "");
    if(db->badTables || db->badKeys) LOGI("Indonesian texts: %d unreadable mission tables, %d unreadable texts skipped", (int)db->badTables, (int)db->badKeys);
    g_idDb = db; g_idState = 2;
    return true;
}
/* self = the game's CText (NULL: MAIN table only). Empty texts of the file count as missing. */
static const unsigned short* IdGxtFind(void* self, const char* key)
{
    if(!key || !key[0] || !IdGxtEnsure()) return NULL;
    const unsigned short* t = g_idDb->Find(key, NULL);
    if(!t && self && fnMissionName)
    {
        char nm[32]; memset(nm, 0, sizeof(nm)); fnMissionName(self, nm); nm[8] = 0;
        if(nm[0]) t = g_idDb->Find(key, nm);
    }
    return (t && t[0]) ? t : NULL;
}
/* texts the file does not have: written once each to files/PCMenu/idgxt_missing.txt, so the translator can see what is still English / Russian... */
static void IdGxtNoteMissing(void* self, const char* key, const unsigned short* game)
{
    static std::mutex m; static std::set<uint32_t> seen; static int lines = 0; static bool started = false;
    std::lock_guard<std::mutex> lk(m);
    if(lines >= 1500 || !seen.insert(gxt::KeyHash(key)).second) return;
    char k[24]; size_t n = 0; for(; key[n] && n < 15; ++n) k[n] = (key[n] >= 33 && key[n] < 127) ? key[n] : '_'; k[n] = 0;
    char nm[32]; memset(nm, 0, sizeof(nm)); if(self && fnMissionName) { fnMissionName(self, nm); nm[8] = 0; for(int i = 0; i < 8; ++i) if(nm[i] && (nm[i] < 33 || nm[i] > 126)) nm[i] = '_'; }
    std::string txt; for(int i = 0; i < 160 && game[i]; ++i) txt += (game[i] >= 32 && game[i] < 127) ? (char)game[i] : '?';
    FILE* f = fopen((g_dirPC + "idgxt_missing.txt").c_str(), started ? "ab" : "wb");
    if(!f) { lines = 1500; return; }
    if(!started) { started = true; fputs("# Texts the game asked for that INDONESIAN.GXT does not have (they are shown in the game's own language)\n# KEY <tab> mission table <tab> the game's text ('?' = a character that is not plain ASCII)\n", f); }
    fprintf(f, "%s\t%s\t%s\n", k, nm[0] ? nm : "MAIN", txt.c_str());
    fclose(f); ++lines;
}
static unsigned short* HookOf_CText_Get(void* self, const char* key)
{
    unsigned short* r = TextGet_orig ? TextGet_orig(self, key) : NULL;
    if(g_cfgLang != LG_ID || !g_cfgIdGxt || !key || !key[0]) return r;          // (the usual case: one comparison and out)
    const unsigned short* t = IdGxtFind(self, key);
    if(t) return (unsigned short*)t;
    if(g_idState.load() == 2 && g_cfgIdGxtLog && r && r[0]) IdGxtNoteMissing(self, key, r);
    return r;
}
static uintptr_t g_vtAction = 0, g_vtSetting = 0, g_vtSettings = 0, g_vtStartGame = 0, g_vtLanguage = 0;
static int g_langStamp = -1, g_langSettle = 0;       // language the rows were built in / frames left in which the game may still be reloading its text
static uintptr_t g_selVts[24]; static int g_selStyle[24]; static const char* g_selTitle[24]; static int g_selVtCount = 0;   // SelectScreen-derived pages the PC menu draws
static uintptr_t g_slotSelVts[4]; static int g_slotSelCount = 0;           // Load/Save/Delete screen row classes (rows named by GetTitle, activated by HandleInput)
static int*  g_pLastDevice = NULL;                                          // `lastDevice` (index into gMobileMenu's pointer state[])
static int   g_style = PS_SETTINGS; static const char* g_slotTitle = "Load Game"; static uintptr_t g_evalVt = 0;
static bool  g_inRender = false, g_slotDirty = false; static int g_slotTick = 0;   // slot titles come from GetTitle(): only ever called from our Render hook (same thread as the stock render)
static std::string g_dlgMsg; static void* g_dlgTop = NULL; static const char* g_dlgKey = NULL;
static std::vector<std::string> g_dlgLines; static float g_dlgMsgTop = 0, g_dlgLineH = 0, g_dlgRowsTop = 0;
static uintptr_t g_gotSettings = 0;
static bool g_pcOptions = false;
static void (*fnSetupLayout)() = NULL;
static void (*fnApplyAudio)() = NULL;
static void (*fnRetune)(void*, signed char) = NULL;
static void (*fnSetHRTF)(void*, int) = NULL;
static void *g_pAudioEngine = NULL, *g_pAEHw = NULL;
static void* g_texBg[10];                // back2..back5 (index = number in the file name)

/* Touch state */
static bool  g_prevDown = false, g_dragging = false;
static float g_pressX = 0, g_pressY = 0, g_lastY = 0;
static int   g_pressRow = -1, g_sliderRow = -1;

/* Layout (recomputed every frame) */
struct Layout
{
    float W, H, ax0, aw, cx;
    float itemScale, titleScale, track, rowStep, firstY, curH, curW, curGap;
    float listTop, listBottom, backY;
    float pItemScale, pRowStep, pListTop, pListBottom, pLabelX, pValueR, pBarW, pOptCx;   // PC options pages
    float pBackL, pBackT, pBackR, pBackB;             // BACK button, bottom right
    float sLabelX, sTop, sStep;                       // Start Game tiles / save slot lists: left aligned like the PC "Game" menu
} L;

static void LayoutDialog();
static void ComputeLayout()
{
    L.W = (float)*(int*)((char*)g_pRsGlobal + OFF_RS_WIDTH);
    L.H = (float)*(int*)((char*)g_pRsGlobal + OFF_RS_HEIGHT);
    L.aw = L.W;                                       // full screen (16:9 / 20:9 phones): no more 4:3 box
    L.ax0 = 0.0f;
    L.cx = L.W * 0.5f;
    float s = g_cfgScale;
    L.itemScale = (0.0215f * L.H * s) / (float)g_bank.capH;
    L.titleScale = (0.0947f * L.H * s) / 64.0f;
    L.track = 0.0022f * L.H;
    g_bank.spaceEx = 0.011f * L.H; g_bank.xs = 1.12f; g_bank.bold = 0.0012f * L.H;
    g_beck.spaceEx = 0.006f * L.H; g_beck.xs = 1.0f; g_beck.bold = 0.0f;
    L.rowStep = 0.0669f * L.H * s;
    L.firstY = 0.336f * L.H;
    L.curH = 0.074f * L.H * s;
    L.curW = 0.0547f * L.H * s;
    L.curGap = 0.019f * L.H;
    L.listTop = 0.29f * L.H;
    L.listBottom = 0.855f * L.H;
    L.backY = 0.915f * L.H;

    L.pItemScale = L.itemScale * 0.92f;
    L.pRowStep = 0.052f * L.H * s;
    L.pListTop = 0.265f * L.H; L.pListBottom = 0.885f * L.H;
    L.pLabelX = 0.105f * L.W; L.pValueR = 0.92f * L.W; L.pBarW = 0.20f * L.W;
    L.pOptCx = 0.36f * L.W;
    L.pBackL = 0.74f * L.W; L.pBackR = L.W; L.pBackT = 0.875f * L.H; L.pBackB = L.H;
    L.sLabelX = 0.075f * L.W; L.sTop = 0.23f * L.H; L.sStep = 0.064f * L.H * s;
    if(g_kind == SK_PAGE && g_style == PS_DIALOG) LayoutDialog();
}

/* Inert mode: while the stock map is visible (MAP view) the hidden stock buttons must not react to taps. */
static int g_inertCount = 0;
static void NoopCb() {}
static void SetStockInert(bool on, void* top)
{
    if(on && !g_inertItems)
    {
        if(!top || top != g_cacheTop || !g_cacheItems || g_stockCount <= 0) return;
        g_inertItems = (StockItem*)g_cacheItems; g_inertTop = top; g_inertCount = g_cacheCount;
        for(int i = 0; i < g_inertCount && i < 32; ++i) g_inertItems[i].cb = NoopCb;
    }
    else if(!on && g_inertItems)
    {
        // restore only if the very same screen/array is still alive
        void* arr = NULL; int cnt = 0; void** scrs = NULL; int sc = 0; void* first = NULL;
        // (MobileMenu pointer is read by the caller through g_mmForInert)
        extern void* g_mmForInert;
        bool alive = g_mmForInert && SafeCopy((char*)g_mmForInert + OFF_MM_SCREENCOUNT, &sc, sizeof(sc)) && sc >= 1 &&
                     SafeCopy((char*)g_mmForInert + OFF_MM_SCREENS, &scrs, sizeof(scrs)) && scrs &&
                     SafeCopy(scrs, &first, sizeof(first)) && first == g_inertTop &&
                     SafeCopy((char*)g_inertTop + OFF_SCR_ITEMS, &arr, sizeof(arr)) && arr == (void*)g_inertItems &&
                     SafeCopy((char*)g_inertTop + OFF_SCR_ITEMCOUNT, &cnt, sizeof(cnt)) && cnt == g_inertCount;
        if(alive) for(int i = 0; i < g_inertCount && i < 32; ++i) g_inertItems[i].cb = g_stock[i].cb;
        g_inertItems = NULL; g_inertTop = NULL; g_inertCount = 0;
    }
}
void* g_mmForInert = NULL;

static const StockItem* FindStock(const char* key)
{
    for(int i = 0; i < g_stockCount; ++i) if(!strcmp(g_stockKeys[i], key)) return &g_stock[i];
    return NULL;
}
static bool IsStandardKey(const char* k)
{
    static const char* known[] = { "FEP_RES", "FEP_STG", "FEH_STA", "FEH_BRI", "FEP_OPT", "FEP_QUI" };
    for(size_t i = 0; i < sizeof(known) / sizeof(known[0]); ++i) if(!strcmp(k, known[i])) return true;
    return false;
}

/* Reads (and caches) the stock item list of the root screen. Returns false if the layout looks wrong. */
static bool ReadStockItems(void* top)
{
    char* scr = (char*)top;
    int count = 0; StockItem* items = NULL;
    if(!SafeCopy(scr + OFF_SCR_ITEMCOUNT, &count, sizeof(count)) || !SafeCopy(scr + OFF_SCR_ITEMS, &items, sizeof(items))) return false;
    if(count < 0 || count > 32 || !items) return false;
    if(g_inertItems && top == g_inertTop && items == g_inertItems) return true;   // callbacks are swapped: keep the originals
    if(top == g_cacheTop && items == g_cacheItems && count == g_cacheCount && ++g_cacheAge < 120) return true;

    int n = 0; StockItem tmp[32]; char keys[32][40];
    for(int i = 0; i < count; ++i)
    {
        if(!SafeCopy(&items[i], &tmp[n], sizeof(StockItem))) return false;
        if(!tmp[n].key || !SafeCString(tmp[n].key, keys[n], sizeof(keys[n]))) keys[n][0] = 0;   // odd/unreadable key: keep the item, no label
        ++n;
    }
    g_stockCount = n;
    for(int i = 0; i < n; ++i) { g_stock[i] = tmp[i]; strcpy(g_stockKeys[i], keys[i]); }
    g_cacheTop = top; g_cacheItems = items; g_cacheCount = count; g_cacheAge = 0;
    if(!g_layoutLogged)
    {
        g_layoutLogged = true;
        LOGI("Root menu screen %p (%s) has %d stock items:", top, g_isPause ? "pause" : "main", n);
        for(int i = 0; i < n; ++i) LOGI("  [%d] key='%s' cb=%p", i, g_stockKeys[i], (void*)g_stock[i].cb);
    }
    return true;
}

/* ---- reading stock FlowScreen tiles of the Options root (SettingsScreen): same layout as the root screen ---- */
struct FlowCache { void* top; void* items; int count; int age; int n; unsigned gen; uintptr_t vt; StockItem it[32]; char keys[32][40]; };
static FlowCache g_optCache = { NULL, NULL, -1, 0, 0, 0 };

static bool ReadFlowItems(void* top, uintptr_t vt, FlowCache& c)
{
    char* scr = (char*)top; int count = 0; StockItem* items = NULL;
    if(!SafeCopy(scr + OFF_SCR_ITEMCOUNT, &count, sizeof(count)) || !SafeCopy(scr + OFF_SCR_ITEMS, &items, sizeof(items))) return false;
    if(count < 0 || count > 32 || !items) return false;
    if(top == c.top && vt == c.vt && items == c.items && count == c.count && ++c.age < 120) return true;   // (a new screen can reuse the freed screen's address: the class is part of the key)
    StockItem tmp[32]; char keys[32][40]; int n = 0;
    for(int i = 0; i < count; ++i)
    {
        if(!SafeCopy(&items[i], &tmp[n], sizeof(StockItem))) return false;
        if(!tmp[n].key || !SafeCString(tmp[n].key, keys[n], sizeof(keys[n]))) keys[n][0] = 0;
        ++n;
    }
    c.n = n; for(int i = 0; i < n; ++i) { c.it[i] = tmp[i]; strcpy(c.keys[i], keys[i]); }
    c.top = top; c.vt = vt; c.items = items; c.count = count; c.age = 0; ++c.gen;
    return true;
}

/* ---- text: a stock row names itself with a GXT key ("FEO_DIS") or, for mods, with plain text ---- */
static std::string GxtToAscii(const unsigned short* w, bool* ok = NULL)      // ok = false when something could not be drawn (shown as '?')
{
    std::string s; bool tok = false, good = true;
    for(int i = 0; i < 160 && w[i]; ++i)
    {
        unsigned c = w[i];
        if(c == '~') { tok = !tok; continue; }          // ~w~ style colour tokens
        if(tok) continue;
        if(!FoldChar(c, s)) good = false;
    }
    if(ok) *ok = good;
    return s;
}
/* Indonesian wording of a GXT key from INDONESIAN.GXT ("" when Indonesian is not the PC menu language, the file or the key is missing, or the text has a
   character the PC menu font cannot draw). Used for what the PC menu's own tables do not know (the tables keep priority: they carry the wording asked for). */
static std::string IdText(const char* key)
{
    if(!g_cfgIdGxt || g_cfgLang != LG_ID || !key || !key[0]) return "";
    const unsigned short* w = IdGxtFind(NULL, key);
    if(!w) return "";
    bool ok = true; std::string s = GxtToAscii(w, &ok);
    return (ok && !s.empty()) ? s : "";
}
static std::string LocalizeText(const std::string& s)    // Indonesian / Arabic PC menu: English wording of the game -> that language (everything else is left alone)
{
    int lg = UiLang();
    if(lg != LG_ID && lg != LG_AR) return s;                  // (fr / de / it / es: the slot names come from the game's GXT in that language already)
    std::string t = PhraseFor(lg, s);
    return t.empty() ? s : t;
}
/* GXT key -> text in the game's language (accents folded). Text without a GXT entry is a mod's own plain text and is returned as it is.
   localize = false returns the game's text untouched by the Indonesian / Arabic layer (for comparisons).
   Text the font cannot draw (Russian / Japanese GXT) is never shown as '?': it becomes the English name of the key (kKeyEn, then the
   guesses of kKeySoft), else the readable part of it, else "" with *drawn = false - the caller then labels the row itself ("OPTION 3"). */
static std::string Resolve(const char* key, bool localize = true, bool* drawn = NULL)
{
    if(drawn) *drawn = true;
    if(!key || !key[0]) return "";
    std::string text = key; bool fromGxt = false, textOK = false;
    if(fnTextGet && g_pTheText)
    {
        if(!g_gxtMissingInit) { g_gxtMissingInit = true; g_gxtMissing = fnTextGet(g_pTheText, "ZZPCMNOKEY"); }
        unsigned short* w = fnTextGet(g_pTheText, key);
        if(w && w != g_gxtMissing)
        {
            bool ok = true; std::string s = GxtToAscii(w, &ok);
            fromGxt = true;
            if(ok && !s.empty()) { text = s; textOK = true; }
            else if(!ok)                                                               // (an empty entry keeps the key as its text, as before)
            {
                const char* en = KeyEnglish(key);
                std::string idt = (en || !localize) ? std::string() : IdText(key);     // Indonesian: the file's own text before any guess
                if(!en && idt.empty()) en = KeyEnglishSoft(key);
                std::string part = (en || !idt.empty()) ? std::string() : DrawablePart(s);
                if(en) text = en;
                else if(!idt.empty()) { text = idt; return text; }
                else if(!part.empty()) text = part;
                else { text.clear(); if(drawn) *drawn = false; }
            }
        }
    }
    int lgU = UiLang();
    bool pcOnly = (lgU == LG_ID || lgU == LG_AR);                                       // languages the game itself does not have
    bool latin  = (lgU >= LG_FR && lgU <= LG_ES);                                       // the game's GXT is already in that language: only a mod's own plain (English) text needs us
    if(localize && !text.empty() && (pcOnly || (latin && !fromGxt)))
    {
        int lg = lgU, gl = GameLanguage();
        bool gameEn = !fromGxt || ((gl == LG_EN || gl < 0) && textOK);                  // the text IS the English wording
        std::string t;
        if(gameEn) t = PhraseFor(lg, text, false);
        if(t.empty() && pcOnly) { const char* k = KeyEnglish(key); if(k) t = PhraseFor(lg, k, false); }            // e.g. tile "LANGUAGE SETUP" (FEO_LAN) -> LANGUAGE
        if(t.empty() && pcOnly && !gameEn) { const char* k = KeyEnglishSoft(key); if(k) t = PhraseFor(lg, k, false); }
        if(t.empty() && !gameEn) t = PhraseFor(lg, text, false);                                       // text already replaced by an English name above
        if(!t.empty()) return t;
        if(lg == LG_ID && fromGxt) { std::string idt = IdText(key); if(!idt.empty()) return idt; }   // no table wording: the Indonesian file's text
        if(gameEn || !textOK) { t = PhraseFor(lg, text, true); if(!t.empty()) return t; }             // last: word by word (new / unknown options), else logged as missing
    }
    return text;
}
static std::string TitleCase(const std::string& in)      // "GAME" -> "Game" (only when the text is all caps)
{
    bool anyLower = false; for(size_t i = 0; i < in.size(); ++i) if(in[i] >= 'a' && in[i] <= 'z') anyLower = true;
    if(anyLower) return in;
    std::string r = in; bool up = true;
    for(size_t i = 0; i < r.size(); ++i)
    {
        char c = r[i];
        if(c >= 'A' && c <= 'Z') { if(!up) r[i] = c + 32; up = false; } else up = (c == ' ');
    }
    return r;
}
static std::string TitleOf(const char* en) { return SmallWords(TitleCase(Tr(en))); }      // backdrop title ("Main Menu") in the PC menu language
static std::string NormName(const std::string& s)        // lower-case letters+digits only, "lib" prefix and ".so/32/64/sa" suffixes removed
{
    std::string r; for(size_t i = 0; i < s.size(); ++i) { char c = s[i]; if(c >= 'A' && c <= 'Z') c += 32; if((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) r += c; }
    if(r.compare(0, 3, "lib") == 0) r.erase(0, 3);
    for(bool again = true; again;)
    {
        again = false;
        static const char* suf[] = { "so", "sa", "32", "64", "arm", "gtasa", "mod" };
        for(size_t k = 0; k < sizeof(suf) / sizeof(suf[0]); ++k)
        {
            size_t n = strlen(suf[k]);
            if(r.size() > n + 3 && r.compare(r.size() - n, n, suf[k]) == 0) { r.erase(r.size() - n); again = true; }
        }
    }
    return r;
}

/* ---- the game's settings table (MobileSettings::settings). SAUtils redirects the pointer, so we always read it through the GOT slot ---- */
struct SView { int value, vmin, vmax, type; const char** names; };
static uint8_t* SettingsBase()
{
    uintptr_t p = 0;
    if(!g_gotSettings || !SafeCopy((void*)g_gotSettings, &p, sizeof(p)) || !p) return NULL;
    return (uint8_t*)p;
}
static bool ReadSetting(int id, SView& v)
{
    if(id < 0 || id > 1023) return false;
    uint8_t* base = SettingsBase(); if(!base) return false;
    uint8_t buf[64];
    if(!SafeCopy(base + (size_t)id * SET_STRIDE, buf, SET_STRIDE)) return false;
    memcpy(&v.value, buf + SET_VALUE, 4); memcpy(&v.vmin, buf + SET_MIN, 4); memcpy(&v.vmax, buf + SET_MAX, 4); memcpy(&v.type, buf + SET_TYPE, 4);
    memcpy(&v.names, buf + SET_NAMES, sizeof(void*));
    return true;
}
static void ApplySideEffect(int id)                      // what SettingSelection::HandleInput does after a change (ids decoded from its jump table)
{
    SView v;
    switch(id)
    {
        case 9:  if(fnSetupLayout) fnSetupLayout(); break;
        case 11: case 12: if(fnApplyAudio) fnApplyAudio(); break;
        case 14: if(fnRetune && g_pAudioEngine && ReadSetting(14, v)) fnRetune(g_pAudioEngine, (signed char)v.value); break;
        case 33: if(fnSetHRTF && g_pAEHw && ReadSetting(33, v)) fnSetHRTF(g_pAEHw, v.value); break;
        default: break;
    }
}
static bool WriteSetting(int id, int val)
{
    SView v; if(!ReadSetting(id, v)) return false;
    volatile int* p = (volatile int*)(SettingsBase() + (size_t)id * SET_STRIDE + SET_VALUE);
    if(*p == val) return false;
    *p = val;
    ApplySideEffect(id);
    return true;
}
static void CycleToggle(int id)                          // same wrap-around as the stock "right pressed" code
{
    SView v; if(!ReadSetting(id, v)) return;
    int nv = v.value + 1; if(v.value >= v.vmax) nv = v.vmin;
    WriteSetting(id, nv);
}
static std::string ToggleText(const SView& v)
{
    char num[24]; snprintf(num, sizeof(num), "%d", v.value);
    int idx = v.value - v.vmin;
    if(!v.names || idx < 0 || idx > 63) return num;
    const char* p = NULL; char key[64];
    if(!SafeCopy(v.names + idx, &p, sizeof(p)) || !p || !SafeCString(p, key, sizeof(key))) return num;
    std::string t = Upper(Resolve(key));
    return t.empty() ? std::string(num) : t;
}

/* ---- save slots (Load / Save / Delete screens) ---- */
static bool IsSlotSelVt(uintptr_t vt) { for(int i = 0; i < g_slotSelCount; ++i) if(g_slotSelVts[i] == vt) return true; return false; }
static std::string SafeGxt(const unsigned short* w)           // GXT text (16 bit chars) without ever faulting on a bad pointer
{
    std::string s; bool tok = false;
    unsigned short c0 = 0;
    if(!w || !SafeCopy(w, &c0, sizeof(c0))) return s;
    bool bytes = (c0 > 0x7f && (c0 & 0xff) >= 32 && (c0 & 0xff) < 127 && ((c0 >> 8) & 0xff) >= 32 && ((c0 >> 8) & 0xff) < 127);   // a plain char* in disguise
    for(int i = 0; i < 120; ++i)
    {
        unsigned c = 0;
        if(bytes) { unsigned char b = 0; if(!SafeCopy((const char*)w + i, &b, 1)) break; c = b; }
        else      { unsigned short u = 0; if(!SafeCopy(w + i, &u, sizeof(u))) break; c = u; }
        if(!c) break;
        if(c == '~') { tok = !tok; continue; }
        if(tok) continue;
        FoldChar(c, s);
    }
    return s;
}
static std::string SlotLabel(void* item, uintptr_t vt)        // MenuSelection::GetTitle() = virtual slot 5 -> GXT text of the slot ("BMX", "EMPTY SLOT", ...)
{
    void* fn = NULL;
    if(!SafeCopy((void*)(vt + 5 * sizeof(void*)), &fn, sizeof(fn)) || !fn) return "";
    const unsigned short* w = ((const unsigned short* (*)(void*))fn)(item);
    return Upper(LocalizeText(SafeGxt(w)));
}
static void RefreshSlotLabels(void* top)                      // only from the Render hook (GetTitle fills a static buffer the stock render uses too)
{
    if(top != g_pageRowsTop) return;
    for(size_t i = 0; i < g_pageRows.size(); ++i)
    {
        PRow& r = g_pageRows[i]; if(r.kind != 3) continue;
        uintptr_t vt = 0; if(!SafeCopy(r.item, &vt, sizeof(vt))) continue;
        if(!IsSlotSelVt(vt)) { g_pageRowsTop = NULL; return; }       // the row is not what it was when the list was built: rebuild before touching it
        std::string t = SlotLabel(r.item, vt);
        if(t.empty()) { char nb[16]; snprintf(nb, sizeof(nb), " %d", (int)i + 1); t = Upper(Tr("SLOT")) + nb; }     // a name the font cannot draw (Cyrillic / Japanese): "SLOT 2"
        r.label = t;
    }
    g_slotDirty = false;
}

/* ---- SelectScreen item list -> rows ---- */
static void BuildPageRows(void* top)
{
    g_pageRows.clear();
    int count = 0; void** arr = NULL;
    g_pageRowsTop = top; g_pageRowsArr = NULL; g_pageRowsCount = -1;
    if(!SafeCopy((char*)top + SEL_ITEMCOUNT, &count, sizeof(count)) || !SafeCopy((char*)top + SEL_ITEMS, &arr, sizeof(arr)) || !arr || count < 0 || count > 160) return;
    int skipped = 0;
    for(int i = 0; i < count; ++i)
    {
        void* it = NULL; uintptr_t vt = 0; const char* tag = NULL; char key[80]; key[0] = 0;
        if(!SafeCopy(arr + i, &it, sizeof(it)) || !it || !SafeCopy(it, &vt, sizeof(vt))) { ++skipped; continue; }
        if(SafeCopy((char*)it + PTR_SZ, &tag, sizeof(tag)) && tag && !SafeCString(tag, key, sizeof(key))) key[0] = 0;
        PRow r; r.item = it; r.setId = -1; r.act = NULL; r.data = 0; r.lastVal = 0x7fffffff;
        if(vt == g_vtAction)
        {
            r.kind = 0;
            SafeCopy((char*)it + ITEM_ONSELECT, &r.act, sizeof(r.act));
            SafeCopy((char*)it + ITEM_ACTDATA, &r.data, sizeof(r.data));
        }
        else if(vt == g_vtSetting)
        {
            int id = -1; SView v;
            if(!SafeCopy((char*)it + ITEM_SETID, &id, sizeof(id)) || !ReadSetting(id, v)) { ++skipped; continue; }
            r.setId = id; r.kind = (v.type == 1) ? 2 : 1;
        }
        else if(IsSlotSelVt(vt)) { r.kind = 3; g_slotDirty = true; }
        else { ++skipped; continue; }                    // e.g. the radio station picker: not supported in the PC page
        if(r.kind == 3) r.label = g_inRender ? SlotLabel(it, vt) : std::string();     // titles are filled in by the Render hook
        else
        {
            bool drawn = true; r.label = Upper(Resolve(key, true, &drawn));
            if(r.label.empty() || !drawn) { char nb[16]; snprintf(nb, sizeof(nb), " %d", (int)g_pageRows.size() + 1); r.label = Upper(Tr("OPTION")) + nb; }   // never '?': "OPTION 3"
        }
        g_pageRows.push_back(r);
    }
    { uintptr_t topVt = 0;                                       // the Language page: more languages, which only the PC menu knows (kind 4, data = the language)
      if(g_vtLanguage && SafeCopy(top, &topVt, sizeof(topVt)) && topVt == g_vtLanguage)
      {
          PRow r; r.item = NULL; r.setId = -1; r.act = NULL; r.data = LG_ID; r.lastVal = 0x7fffffff; r.kind = 4; r.label = "BAHASA INDONESIA"; g_pageRows.push_back(r);
          if(g_arOK) { r.data = LG_AR; r.label = "\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A\xD8\xA9"; g_pageRows.push_back(r); }     // "Al-Arabiyya" in Arabic script
      } }
    g_pageRowsArr = arr; g_pageRowsCount = count;
    LOGI("Settings page '%s': %d row(s) shown, %d item(s) not supported", g_pageTitle.c_str(), (int)g_pageRows.size(), skipped);
}
static void RefreshPageRows(void* top)                   // rebuild when the stock screen's item list changed
{
    int count = 0; void** arr = NULL;
    if(!SafeCopy((char*)top + SEL_ITEMCOUNT, &count, sizeof(count)) || !SafeCopy((char*)top + SEL_ITEMS, &arr, sizeof(arr))) return;
    if(top != g_pageRowsTop || arr != g_pageRowsArr || count != g_pageRowsCount) BuildPageRows(top);
}
static void BuildOptRows()
{
    g_optRows.clear();
    for(int i = 0; i < g_optCache.n; ++i)
    {
        if(!g_optCache.it[i].cb || !g_optCache.keys[i][0]) continue;
        OptRow r; r.title = TitleCase(Resolve(g_optCache.keys[i]));
        if(r.title.empty()) { char nb[16]; snprintf(nb, sizeof(nb), " %d", (int)g_optRows.size() + 1); r.title = TitleCase(Tr("OPTION")) + nb; }
        r.label = Upper(r.title); r.cb = g_optCache.it[i].cb; r.idx = i;
        g_optRows.push_back(r);
    }
    g_optRowsGen = g_optCache.gen;
}

static int PageVtIndex(uintptr_t vt) { for(int i = 0; i < g_selVtCount; ++i) if(g_selVts[i] == vt) return i; return -1; }

/* ---- Yes/No and OK dialogs (YesNoScreen / OkScreen): message + centered buttons ---- */
static std::vector<std::string> WrapText(const std::string& t, float scale, float maxW)
{
    std::vector<std::string> lines; std::string cur;
    size_t i = 0;
    while(i < t.size())
    {
        size_t e = t.find(' ', i); if(e == std::string::npos) e = t.size();
        std::string w = t.substr(i, e - i); i = e + 1;
        if(w.empty()) continue;
        std::string cand = cur.empty() ? w : cur + " " + w;
        if(!cur.empty() && g_bank.Width(cand, scale, L.track) > maxW) { lines.push_back(cur); cur = w; } else cur = cand;
    }
    if(!cur.empty()) lines.push_back(cur);
    return lines;
}
static void PrepareDialog(void* top)                            // reads the message key the stock ConfirmationScreen::Render shows
{
    const char* key = NULL;
    if(!SafeCopy((char*)top + OFF_MSG, &key, sizeof(key))) key = NULL;
    if(top == g_dlgTop && key == g_dlgKey) return;
    g_dlgTop = top; g_dlgKey = key; g_dlgMsg.clear();
    char kb[96];
    if(key && SafeCString(key, kb, sizeof(kb)))
    {
        std::string m = Resolve(kb);
        if(m != kb || strchr(kb, ' ')) g_dlgMsg = Upper(m);      // an unresolved key (not text, not in the GXT) is better left out than shown
        LOGI("Dialog %p: message key '%s' -> '%s'", top, kb, g_dlgMsg.c_str());
    }
    else LOGI("Dialog %p: message key unreadable, showing buttons only", top);
}
static void LayoutDialog()
{
    g_dlgLines = WrapText(g_dlgMsg, L.itemScale, 0.62f * L.W);
    g_dlgLineH = 0.062f * L.H; g_dlgMsgTop = 0.30f * L.H;
    g_dlgRowsTop = std::max(0.50f * L.H, g_dlgMsgTop + g_dlgLineH * (float)g_dlgLines.size() + 0.06f * L.H);
}

/* "Change button position" (first row of Controls) is not a screen: CAdjustableHUD::Toggle() creates CAdjustableHUD::m_pInstance
   and from then on the stock MobileMenu::Update only runs that editor (and returns) while MobileMenu::Render draws it over the
   blurred background. The Controls page stays on top of the screen stack the whole time, so painting the PC page over it and
   blinding the pointers (what a taken-over screen gets) made the editor look like it never opened and left it untouchable.
   While the editor is up the stock UI owns drawing and input; the PC page comes back when it is closed. */
static bool AdjustableHUDUp()
{
    void* p = NULL;
    if(!g_ppAdjHUD || !SafeCopy(g_ppAdjHUD, &p, sizeof(p))) return false;
    return p != NULL;
}

/* Decides which stock screen (if any) the PC menu replaces this frame. */
static int EvalTakeover(void* mm, void** outTop)
{
    g_editorUp = false;
    if(!g_cfgEnabled) return SK_NONE;
    if(!g_resOK) { static bool l = false; if(!l && g_resTried) { l = true; LOGE("PC menu inactive: font atlases did not load"); } return SK_NONE; }
    {   // a language change (Language page, ini, or the game reloading its text a few frames later): rebuild every list that holds texts
        int st = LangStamp(); bool again = false;
        if(st != g_langStamp) { g_langStamp = st; g_langSettle = 24; again = true; LOGI("Language: game %d, PC menu %d", GameLanguage(), UiLang()); }
        else if(g_langSettle > 0 && --g_langSettle % 8 == 0) again = true;
        if(again)
        {
            g_optRowsGen = 0xFFFFFFFFu; g_pageRowsTop = NULL; g_dlgTop = NULL;       // (a dialog screen at the same address with the same key would keep its text of the old language)
            if(g_vtLanguage && g_activeVt == g_vtLanguage) g_pageTitle = SmallWords(TitleCase(Resolve("FEO_LAN")));
        }
    }
    char* m = (char*)mm;
    int count = 0; void** arr = NULL; void* top = NULL;
    if(!SafeCopy(m + OFF_MM_SCREENCOUNT, &count, sizeof(count))) return SK_NONE;
    { static int lastCount = -2; if(count != lastCount) { lastCount = count; LOGI("MobileMenu screen count = %d", count); } }
    if(count < 1 || count > 8) return SK_NONE;
    if(!SafeCopy(m + OFF_MM_SCREENS, &arr, sizeof(arr)) || !arr) return SK_NONE;
    if(!SafeCopy(arr + (count - 1), &top, sizeof(top)) || !top) return SK_NONE;      // the top screen is screens[count-1]
    uintptr_t vt = 0;
    if(!SafeCopy(top, &vt, sizeof(vt))) return SK_NONE;
    g_mmForInert = mm; g_evalVt = vt;
    if(AdjustableHUDUp())                                                            // the stock button-position editor is open
    {
        g_editorUp = true;
        static bool l = false; if(!l) { l = true; LOGI("Button position editor is up: the stock UI owns the screen until it is closed"); }
        return SK_NONE;
    }

    if(count == 1 && vt == g_vtMainMenu)                                             // title / pause root
    {
        uint8_t pauseByte = 0;
        if(!SafeCopy((char*)top + OFF_SCR_ISPAUSE, &pauseByte, 1)) return SK_NONE;
        g_isPause = pauseByte != 0;
        if(!ReadStockItems(top))
        {
            if(!g_badLayoutLogged) { g_badLayoutLogged = true; LOGE("Stock menu item list failed validation: falling back to the stock menu."); }
            return SK_NONE;
        }
        *outTop = top; return SK_ROOT;
    }
    if(g_pcOptions && count >= 2 && (vt == g_vtSettings || (g_vtStartGame && vt == g_vtStartGame)))     // stock Options (Game, Controls, Audio...) / Start Game (New, Load, Delete)
    {
        if(!ReadFlowItems(top, vt, g_optCache)) return SK_NONE;
        if(g_optRowsGen != g_optCache.gen) BuildOptRows();
        if(g_optRows.empty()) return SK_NONE;
        *outTop = top;
        if(g_vtStartGame && vt == g_vtStartGame) { g_style = PS_TILES_LEFT; return SK_START; }
        g_style = PS_TILES_CENTER; return SK_OPTIONS;
    }
    int pv = (g_pcOptions && count >= 2) ? PageVtIndex(vt) : -1;                     // one of its pages (a Save screen that is the ONLY screen, i.e. at a save point, stays stock)
    if(pv >= 0)
    {
        g_style = g_selStyle[pv]; g_slotTitle = g_selTitle[pv] ? g_selTitle[pv] : "";
        if(g_style != PS_SETTINGS)                                                   // slot lists and Yes/No dialogs need their rows: without them the stock screen stays usable
        {
            RefreshPageRows(top);
            if(g_style == PS_DIALOG) PrepareDialog(top);
            if(g_pageRows.empty()) { static uintptr_t warned = 0; if(warned != vt) { warned = vt; LOGE("Screen %p has no rows the PC menu understands: stock UI stays", (void*)vt); } return SK_NONE; }
        }
        *outTop = top; return SK_PAGE;
    }
    { static uintptr_t seen[8]; static int ns = 0; bool known = false; for(int i = 0; i < ns; ++i) if(seen[i] == vt) known = true;
      if(!known && ns < 8) { seen[ns++] = vt; LOGI("Screen with vtable %p (stack depth %d) is not replaced: stock UI", (void*)vt, count); } }
    return SK_NONE;
}

/* Main / pause menu label. English game: the PC wording. fr/de/it/es: the game's own text for the stock key (so it says what the
   stock menu says), our table when that is missing. Indonesian: the table. Russian / Japanese: English (no glyphs). */
static std::string MenuLabel(const char* en, const char* key, bool tableOnly = false)
{
    int lg = UiLang();
    if(lg == LG_EN || lg == LG_RU || lg == LG_JA) return en;
    if(lg != LG_ID && lg != LG_AR && !tableOnly && key && key[0] && fnTextGet && g_pTheText)
    {
        if(!g_gxtMissingInit) { g_gxtMissingInit = true; g_gxtMissing = fnTextGet(g_pTheText, "ZZPCMNOKEY"); }
        unsigned short* w = fnTextGet(g_pTheText, key);
        if(w && w != g_gxtMissing) { bool ok = true; std::string t = GxtToAscii(w, &ok); if(ok && !t.empty()) return Upper(t); }
    }
    return Tr(en);
}

static std::string OptionsTitle() { return SmallWords(TitleCase(MenuLabel("OPTIONS", "FEP_OPT"))); }

static void BuildEntries()
{
    g_entries.clear();
    #define PUSH(_en, _kind, _key, _fb, _tbl) { Entry e; e.label = MenuLabel(_en, _key, _tbl); if(_kind == K_MAP) e.label = MenuLabel("MAP", "FEH_MAP"); if(_kind == K_MODS) e.label = Tr("MODS"); e.kind = _kind; e.key = _key; e.fallback = _fb; g_entries.push_back(e); }
    if(g_isPause)
    {
        PUSH("RESUME",         K_STOCK, "FEP_RES", fnOnResume,    false);
        PUSH("START NEW GAME", K_STOCK, "FEP_STG", fnOnStartGame, true);
        PUSH("MAP",            K_MAP,   "",        NULL,          false);
        PUSH("STATS",          K_STOCK, "FEH_STA", fnOnStats,     false);
        PUSH("BRIEF",          K_STOCK, "FEH_BRI", fnOnBriefs,    false);
        PUSH("OPTIONS",        K_STOCK, "FEP_OPT", fnOnSettings,  false);
        if(g_cfgModsInMain) PUSH("MODS", K_MODS, "", NULL, false);
        PUSH("QUIT GAME",      K_STOCK, "FEP_QUI", fnOnExit,      false);
    }
    else
    {
        if(FindStock("FEP_RES")) PUSH("RESUME", K_STOCK, "FEP_RES", fnOnResume, false);
        PUSH("START GAME", K_STOCK, "FEP_STG", fnOnStartGame, false);
        PUSH("OPTIONS",    K_STOCK, "FEP_OPT", fnOnSettings,  false);
        if(g_cfgModsInMain) PUSH("MODS", K_MODS, "", NULL, false);
        PUSH("QUIT GAME",  K_STOCK, "FEP_QUI", fnOnExit,      false);
    }
    #undef PUSH
    if(g_sel >= (int)g_entries.size()) g_sel = 0;
}

static std::string ModDisplayName(const std::string& file, bool isDir)
{
    std::string n = file;
    if(!isDir)
    {
        size_t dot = n.rfind('.');
        if(dot != std::string::npos && dot > 0)
        {
            std::string ext = Upper(n.substr(dot));
            n = n.substr(0, dot);
            if(ext == ".SO" && n.size() > 3 && (n.compare(0, 3, "lib") == 0 || n.compare(0, 3, "Lib") == 0)) n = n.substr(3);
        }
    }
    return Upper(n);
}

static void RefreshModRows()
{
    g_modRows.clear();
    // 0) one entry that opens the configuration page of every mod (the stock "Mods settings" screen, shown PC style)
    if(g_pcOptions && fnOnSettings)
    {
        ModRow r; r.label = Tr("MODS SETTINGS"); r.file = Tr("OPEN THE CONFIGURATION OF YOUR MODS"); r.cb = NULL; r.kind = MR_SETTINGS; r.idx = -1;
        g_modRows.push_back(r);
    }
    // 1) settings tabs that other mods added to the stock menu
    for(int i = 0; i < g_stockCount; ++i)
    {
        if(!g_stockKeys[i][0] || !g_stock[i].cb || IsStandardKey(g_stockKeys[i])) continue;
        ModRow r; r.label = Upper(g_stockKeys[i]); r.file = Tr("SETTINGS") + ": " + g_stockKeys[i]; r.cb = g_stock[i].cb; r.kind = MR_TAB; r.idx = i;
        g_modRows.push_back(r);
    }
    int head = (int)g_modRows.size();
    // 2) everything inside Android/data/<pkg>/mods/
    std::vector<ModRow> mods;
    if(DIR* d = opendir(g_dirMods.c_str()))
    {
        while(dirent* e = readdir(d))
        {
            if(e->d_name[0] == '.') continue;
            std::string full = g_dirMods + e->d_name;
            bool isDir = (e->d_type == DT_DIR);
            if(e->d_type == DT_UNKNOWN) { struct stat st; if(stat(full.c_str(), &st) == 0) isDir = S_ISDIR(st.st_mode); }
            ModRow r; r.label = ModDisplayName(e->d_name, isDir); r.file = e->d_name; r.cb = NULL; r.kind = MR_FILE; r.idx = -1;
            mods.push_back(r);
        }
        closedir(d);
    }
    std::sort(mods.begin(), mods.end(), [](const ModRow& a, const ModRow& b) { return a.label < b.label; });
    g_modRows.insert(g_modRows.end(), mods.begin(), mods.end());
    LOGI("Mods view: %d entry(ies) above the list, %d mod file(s) in %s", head, (int)mods.size(), g_dirMods.c_str());
}

/* The game changes screens in two steps: OnBack / a tile's callback only store the target in MobileMenu+pending,
   and MobileMenu::Update applies it later, once the screen's transition has finished. Asking for another change in
   between makes the game apply the first one early AND the second one (one screen too many: the root menu was popped,
   the menu closed and the game was left on a black screen). So nothing that changes screens runs while one is pending. */
static bool MenuPending()
{
    void* p = NULL;
    if(!g_mmForInert || !SafeCopy((char*)g_mmForInert + OFF_MM_PENDING, &p, sizeof(p))) return true;   // unreadable: be conservative
    return p != NULL;
}
static int MenuDepth()
{
    int c = 0;
    if(!g_mmForInert || !SafeCopy((char*)g_mmForInert + OFF_MM_SCREENCOUNT, &c, sizeof(c))) return 0;
    return c;
}
static bool InGame() { return fnIsPlaying ? fnIsPlaying(true) : true; }   // Stats / Brief / Map read the player: no game = null pointer = crash

/* The stock menu has a "map mode" byte (MobileMenu+0x6D / +0x8D): the full-screen, scrollable map. While it is set,
   MobileMenu::Update does NOT update the top screen and MobileMenu::Render does NOT draw it (unless its fade value is > 0),
   and only a FlowScreen can leave the mode again. The game switches it on by itself (InitForPause when the radar widget is
   visible, a quick tap on the map) and the stock buttons are hidden while it is on, so the stock UI can never push another
   screen in that state. Our PC buttons can: Stats / Brief (SelectScreen, not a FlowScreen) opened in map mode were never
   drawn nor updated again - blank screen, no way back - until the map was opened and closed once more.
   So the PC menu owns this byte: it is only ever on while our MAP view is up, and a non-flow screen on top clears it. */
static bool TopIsFlow(void* mm)
{
    char* m = (char*)mm; int count = 0; void** arr = NULL; void* top = NULL; uintptr_t vt = 0; void* fn = NULL;
    if(!SafeCopy(m + OFF_MM_SCREENCOUNT, &count, sizeof(count)) || count < 1 || count > 8) return true;     // unknown: behave as "flow" = do nothing
    if(!SafeCopy(m + OFF_MM_SCREENS, &arr, sizeof(arr)) || !arr || !SafeCopy(arr + (count - 1), &top, sizeof(top)) || !top) return true;
    if(!SafeCopy(top, &vt, sizeof(vt)) || !vt || !SafeCopy((void*)(vt + 11 * sizeof(void*)), &fn, sizeof(fn)) || !fn) return true;   // virtual slot 11 = MenuScreen::IsFlow
    return ((bool (*)(void*))fn)(top);
}
static void SetMapMode(void* mm, uint8_t v)
{
    uint8_t cur = 0;
    if(mm && SafeCopy((char*)mm + OFF_MM_MAPMODE, &cur, 1) && cur != v) *(volatile uint8_t*)((char*)mm + OFF_MM_MAPMODE) = v;
}
static void EnforceMapMode(void* mm, int kind)
{
    uint8_t v = 0;
    if(!mm || !SafeCopy((char*)mm + OFF_MM_MAPMODE, &v, 1) || v == 0) return;
    bool clear = (kind == SK_NONE) ? !TopIsFlow(mm)                                  // Stats / Brief / any other non-flow screen on top
                                   : !(kind == SK_ROOT && g_view == VIEW_MAP);       // every PC screen except our own MAP view
    if(!clear) return;
    *(volatile uint8_t*)((char*)mm + OFF_MM_MAPMODE) = 0;
    static int logged = 0;
    if(logged < 6) { ++logged; LOGI("Stock map mode was on while screen kind %d was up: switched off (Stats / Brief would stay blank)", kind); }
}

/* The stock UI sets FlowScreen::m_nChosenButton to the tile it is activating, and mod tile callbacks (SAUtils'
   OnTabButtonClicked / OnCustomModSettingsOpened take no argument) read it to find out WHICH tab was clicked.
   Calling a tile callback without it made them use a stale index and run the wrong (garbage) function. */
static void CallTile(void* screen, int idx, VoidFn cb)
{
    int cur = 0;
    if(screen && idx >= 0 && SafeCopy((char*)screen + OFF_SCR_CHOSEN, &cur, sizeof(cur)))
        *(volatile int*)((char*)screen + OFF_SCR_CHOSEN) = idx;
    if(cb) cb();
}
static int FindStockIdx(const char* key) { for(int i = 0; i < g_stockCount; ++i) if(!strcmp(g_stockKeys[i], key)) return i; return -1; }

static void SetToast(const std::string& t) { g_toast = t; g_toastFrames = 240; }

/* The gang territory on the map is drawn by the game itself (Menu_MapRender -> CRadar::DrawRadarGangOverlay(true)), which the
   PC menu never replaces: it runs inside the stock MobileMenu::Render exactly as before. The game skips it completely while
   CGangWars::bGangWarsActive is 0 (new game; the story switches it on later). Only logged, for diagnosis. */
static void NoteGangTerritory()
{
    uint8_t on = 1; uint16_t zones = 0;
    if(!g_pGangWars || !SafeCopy(g_pGangWars, &on, 1)) return;
    if(g_pNavZones) SafeCopy(g_pNavZones, &zones, sizeof(zones));
    LOGI("Map opened: gang wars %s in this save, %u navigation zones (the game draws the territory itself, only while gang wars are active)", on ? "ACTIVE" : "NOT active", (unsigned)zones);
}

/* ------------------------------------------------------------------------------------------ */
/*  Actions                                                                                     */
/* ------------------------------------------------------------------------------------------ */
static void OpenView(int v)
{
    g_view = v; g_scroll = 0; g_sel = 0; g_dragging = false;
    if(v == VIEW_MODS) RefreshModRows();
}
static void ActivateMain(int idx)
{
    if(idx < 0 || idx >= (int)g_entries.size()) return;
    const Entry& e = g_entries[idx];
    bool needsGame = (e.kind == K_MAP) || !strcmp(e.key, "FEH_STA") || !strcmp(e.key, "FEH_BRI");
    if(needsGame && !InGame()) { SndPlay(g_sndBack); SetToast(Tr("NOT AVAILABLE WITHOUT A RUNNING GAME")); LOGI("'%s' refused: no running game", e.label.c_str()); return; }
    SndPlay(g_sndSelect);
    if(e.kind == K_MAP) { g_view = VIEW_MAP; SetMapMode(g_mmForInert, 1); NoteGangTerritory(); return; }   // the stock map becomes visible and scrollable at once
    if(e.kind == K_MODS) { OpenView(VIEW_MODS); return; }
    const StockItem* it = FindStock(e.key);
    VoidFn cb = it ? it->cb : e.fallback;
    if(MenuPending()) return;                                      // the previous screen change has not finished yet
    SetMapMode(g_mmForInert, 0);                                   // stock buttons only exist outside the map mode: the screen they open must start in that state
    if(cb) { LOGI("Menu action '%s' -> %p", e.label.c_str(), (void*)cb); CallTile(g_activeTop, it ? FindStockIdx(e.key) : -1, cb); }
}

/* The stock "Back" of the current screen (virtual slot 9 = OnBack in every MenuScreen), so saving / popping behaves exactly as stock.
   Returns false (and does nothing) when it must not run. */
static bool DoBack(bool sound)
{
    void* top = g_activeTop; uintptr_t vt = 0; void* fn = NULL;
    if(!top) return false;
    if(MenuDepth() < 2) { LOGI("Back ignored: it would close the menu"); return false; }      // never pop the root screen
    if(MenuPending())   { LOGI("Back ignored: a screen change is still in progress"); return false; }
    if(!SafeCopy(top, &vt, sizeof(vt)) || !SafeCopy((void*)(vt + 9 * sizeof(void*)), &fn, sizeof(fn)) || !fn) return false;
    if(sound) SndPlay(g_sndBack);
    LOGI("PC Back -> OnBack of screen %p (%p)", top, fn);
    ((void (*)(void*))fn)(top);
    return true;
}

/* Mods view -> (real) Options screen -> "Mods settings" page, all without showing the stock UI. */
static void StartNav(const std::string& filter)
{
    if(!fnOnSettings || g_nav != NAV_NONE || MenuPending()) return;
    g_autoBack = false;
    g_nav = NAV_WAIT_OPTIONS; g_navFrames = 0; g_navFilter = filter; g_returnToMods = true;
    g_modsSelSaved = g_sel; g_modsScrollSaved = g_scroll;
    g_pendingRootView = VIEW_MODS;                                   // when we land on the root screen again, reopen the Mods list
    g_pageTitle = SmallWords(TitleCase(Tr("MODS SETTINGS"))); g_pageBg = 2;
    LOGI("Opening mod settings (filter '%s')", filter.c_str());
    fnOnSettings();
}
static void AbortNav(const char* why, void* optionsTop)
{
    LOGE("Mod settings navigation stopped: %s", why);
    g_nav = NAV_NONE;
    SetToast(Tr("COULD NOT OPEN MOD SETTINGS"));
    g_returnToMods = (optionsTop != NULL);                         // when we are on the Options screen, the frame loop leaves it (once, when no change is pending)
    g_pendingRootView = VIEW_MODS;
}
static void JumpToFilter()                                          // find the section of the chosen mod inside the settings page
{
    if(g_navFilter.empty() || g_pageRows.empty()) return;
    std::string f = NormName(g_navFilter);
    if(f.size() >= 3)
    {
        for(size_t i = 0; i < g_pageRows.size(); ++i)
        {
            std::string r = NormName(g_pageRows[i].label);
            if(r.size() >= 3 && (r.find(f) != std::string::npos || (r.size() >= 5 && f.find(r) != std::string::npos)))
            {
                g_sel = (int)i; g_scroll = std::max(0.0f, L.pRowStep * (float)i - L.pRowStep);
                return;
            }
        }
    }
    SetToast(Tr("NO SETTINGS FOUND FOR") + " " + Upper(g_navFilter) + " - " + Tr("SHOWING ALL"));
}
/* runs once per frame after the stock Update */
static void RunNav(int kind, void* top)
{
    if(g_nav == NAV_NONE) return;
    if(++g_navFrames > 300) { AbortNav("timeout", kind == SK_OPTIONS ? top : NULL); return; }
    if(g_nav == NAV_WAIT_OPTIONS && kind == SK_OPTIONS)
    {
        int idx = -1;
        for(int i = 0; i < g_optCache.n && idx < 0; ++i) if(NormName(g_optCache.keys[i]) == "modssettings" || NormName(Resolve(g_optCache.keys[i], false)) == "modssettings") idx = i;
        for(int i = 0; i < g_optCache.n && idx < 0; ++i) if(NormName(g_optCache.keys[i]).find("mod") != std::string::npos && g_optCache.it[i].cb) idx = i;
        if(idx < 0 || !g_optCache.it[idx].cb) { AbortNav("no 'Mods settings' button on the Options screen (is SAUtils installed?)", top); return; }
        if(MenuPending()) return;                                    // wait until the Options screen is fully shown
        LOGI("Mods settings button: key '%s' cb %p", g_optCache.keys[idx], (void*)g_optCache.it[idx].cb);
        g_nav = NAV_WAIT_PAGE;
        CallTile(top, idx, g_optCache.it[idx].cb);
    }
    else if(g_nav == NAV_WAIT_PAGE && kind == SK_PAGE)
    {
        g_nav = NAV_NONE;
        BuildPageRows(top);
        g_sel = 0; g_scroll = 0; ComputeLayout();
        JumpToFilter();
    }
}
/* ------------------------------------------------------------------------------------------ */
/*  Input                                                                                       */
/* ------------------------------------------------------------------------------------------ */
static int RowAtMain(float y)
{
    float r = (y - (L.firstY - L.rowStep * 0.5f)) / L.rowStep;
    int i = (int)floorf(r);
    return (i >= 0 && i < (int)g_entries.size()) ? i : -1;
}
static int RowAtMods(float y)
{
    if(y >= L.backY - L.rowStep * 0.5f && y <= L.backY + L.rowStep * 0.5f) return (int)g_modRows.size();   // BACK
    if(y < L.listTop || y > L.listBottom) return -1;
    int i = (int)floorf((y - L.listTop + g_scroll) / L.rowStep);
    return (i >= 0 && i < (int)g_modRows.size()) ? i : -1;
}
static float MaxScroll() { return std::max(0.0f, L.rowStep * (float)g_modRows.size() - (L.listBottom - L.listTop)); }

/* geometry of the Options root (centered tiles) and of the settings pages (label left, value right) */
static bool  IsTileKind()    { return g_kind == SK_OPTIONS || g_kind == SK_START; }     // pages made of the stock FlowScreen tiles
static int   PageCount()     { return IsTileKind() ? (int)g_optRows.size() : (int)g_pageRows.size(); }
static float PageWinTop()
{
    switch(g_style) { case PS_TILES_CENTER: return L.firstY - L.rowStep * 0.5f; case PS_TILES_LEFT: return L.sTop; case PS_DIALOG: return g_dlgRowsTop; default: return L.pListTop; }
}
static float PageStep()
{
    switch(g_style) { case PS_TILES_CENTER: return L.rowStep; case PS_TILES_LEFT: return L.sStep; case PS_DIALOG: return L.rowStep; default: return L.pRowStep; }
}
static float PageWinBottom()
{
    switch(g_style) { case PS_TILES_CENTER: case PS_TILES_LEFT: return 0.88f * L.H; case PS_DIALOG: return g_dlgRowsTop + L.rowStep * (float)PageCount(); default: return L.pListBottom; }
}
static float PageRowCY(int i){ return PageWinTop() + PageStep() * ((float)i + 0.5f) - g_scroll; }
static float PageMaxScroll() { return std::max(0.0f, PageStep() * (float)PageCount() - (PageWinBottom() - PageWinTop())); }
static int   RowAtPage(float y)
{
    if(y < PageWinTop() || y > PageWinBottom()) return -1;
    int i = (int)floorf((y - PageWinTop() + g_scroll) / PageStep());
    return (i >= 0 && i < PageCount()) ? i : -1;
}

/* The real finger state, sampled BEFORE the stock menu is blinded (see MaskPointers). */
struct Touch { bool down; int x, y; };
static void ReadTouch(Touch& t)
{
    t.down = false; t.x = t.y = 0;
    int st = fnPtrButton(0, 0);
    t.down = (st == 2 || st == 3);          // 2 = pressed this frame, 3 = held
    fnPtrCoords(0, &t.x, &t.y, NULL);
}

/* The stock menu reads touches from the game's `pointers` table (LIB_PointerGetButton just indexes it).
   Hooking that tiny function is unsafe on Thumb (it starts with an IT block), so instead we zero the button
   words of the table for the duration of the stock Update call and put them back afterwards. */
static uint32_t g_savedPtr[2][3];
static int      g_savedXY[2][2];
static bool     g_ptrMasked[2] = { false, false };
static const int OFFSCREEN = -30000;
/* The stock menu decides "Back pressed" from the pointer state AND the pointer's stored coordinates (BackPressed():
   state == up and coordinates inside the stock Back button). Zeroing only the state left the coordinates at the last
   finger position, so after a tap on our BACK (same corner as the stock one) the stock code kept calling OnBack every
   frame and popped screen after screen. So the stock menu now sees "no finger" AND coordinates far outside the screen. */
static void MaskPointers()
{
    for(int p = 0; p < 2; ++p)                // the game only ever uses pointers 0 and 1
    {
        g_ptrMasked[p] = false;
        if(!g_pointers) continue;
        if(g_kind == SK_ROOT && g_view == VIEW_MAP) continue;   // map view: the stock map (scrolling, its own Back button bottom right) gets the real finger
        volatile uint32_t* w = (volatile uint32_t*)(g_pointers + (size_t)p * PTR_STRIDE);
        volatile int* xy = (volatile int*)(g_pointers + (size_t)p * PTR_STRIDE + PTR_X);   // x at +0xC, y at +0x10
        for(int b = 0; b < 3; ++b) { g_savedPtr[p][b] = w[b]; w[b] = 0; }
        g_savedXY[p][0] = xy[0]; g_savedXY[p][1] = xy[1]; xy[0] = OFFSCREEN; xy[1] = OFFSCREEN;
        g_ptrMasked[p] = true;
    }
}
static void UnmaskPointers()
{
    for(int p = 0; p < 2; ++p)
    {
        if(!g_ptrMasked[p]) continue;
        volatile uint32_t* w = (volatile uint32_t*)(g_pointers + (size_t)p * PTR_STRIDE);
        volatile int* xy = (volatile int*)(g_pointers + (size_t)p * PTR_STRIDE + PTR_X);
        for(int b = 0; b < 3; ++b) if(w[b] == 0) w[b] = g_savedPtr[p][b];   // don't clobber an event the input thread wrote meanwhile
        if(xy[0] == OFFSCREEN) xy[0] = g_savedXY[p][0];
        if(xy[1] == OFFSCREEN) xy[1] = g_savedXY[p][1];
        g_ptrMasked[p] = false;
    }
}

static void SliderSetFromX(int row, float x)
{
    if(row < 0 || row >= (int)g_pageRows.size()) return;
    SView v; if(!ReadSetting(g_pageRows[row].setId, v)) return;
    float barL = IsRTL() ? L.W - L.pValueR : L.pValueR - L.pBarW, barR = barL + L.pBarW;
    float f = (x - barL) / (barR - barL); if(IsRTL()) f = 1.0f - f; f = f < 0 ? 0 : (f > 1 ? 1 : f);
    long nv = (long)v.vmin + (long)floorf(f * (float)((long)v.vmax - (long)v.vmin) + 0.5f);
    if(nv < v.vmin) nv = v.vmin; if(nv > v.vmax) nv = v.vmax;
    WriteSetting(g_pageRows[row].setId, (int)nv);
}

/* Load / Save / Delete rows have no OnSelect pointer: the row's own HandleInput (virtual slot 6) does the work (confirm dialog or direct load).
   It only acts while the game's pointer state for the last used device is 0 ("released"), so that word is cleared for the duration of the call. */
static void CallSelectionInput(void* item, void* screen)
{
    uintptr_t vt = 0; void* fn = NULL;
    if(!item || !SafeCopy(item, &vt, sizeof(vt)) || !IsSlotSelVt(vt) || !SafeCopy((void*)(vt + 6 * sizeof(void*)), &fn, sizeof(fn)) || !fn) return;
    volatile int* ps = NULL; int saved = 0;
    int dev = 0;
    if(g_mmForInert && g_pLastDevice && SafeCopy(g_pLastDevice, &dev, sizeof(dev)) && dev >= 0 && dev < 4)
    {
        ps = (volatile int*)((char*)g_mmForInert + MM_PSTATE + dev * 4);
        if(!SafeCopy((void*)ps, &saved, sizeof(saved))) ps = NULL;
    }
    if(ps) *ps = 0;
    ((void (*)(void*, void*, float))fn)(item, screen, 0.016f);
    if(ps && *ps == 0 && saved != 0) *ps = saved;
}

static void ActivatePageRow(int i)
{
    if(IsTileKind())
    {
        if(i < 0 || i >= (int)g_optRows.size() || MenuPending()) return;
        SndPlay(g_sndSelect);
        g_pageTitle = SmallWords(g_optRows[i].title);
        { const char* k = g_optRows[i].idx >= 0 && g_optRows[i].idx < g_optCache.n ? g_optCache.keys[g_optRows[i].idx] : "";
          g_pageBg = !strcmp(k, "FEO_AUD") ? 5 : !strcmp(k, "FEO_DIS") ? 3 : (!strcmp(k, "FEC_RED") || !strcmp(k, "FEO_CON")) ? 2 : (!strcmp(k, "FEH_LOA") || !strcmp(k, "FEO_GAM") || !strcmp(k, "FEO_LAN")) ? 4 : -1; }   // (the real tile keys: Game = FEH_LOA, Controls = FEC_RED)
        VoidFn cb = g_optRows[i].cb;
        LOGI("Options -> '%s' (tile %d, %p)", g_optRows[i].title.c_str(), g_optRows[i].idx, (void*)cb);
        if(cb) CallTile(g_activeTop, g_optRows[i].idx, cb);
        return;
    }
    if(i < 0 || i >= (int)g_pageRows.size()) return;
    PRow& r = g_pageRows[i];
    if(r.kind == 4) { SndPlay(g_sndSelect); if(g_cfgLang != r.data) { g_cfgLang = r.data; SaveLanguageSetting(); LOGI("PC menu language: %s", r.data == LG_AR ? "Arabic" : "Indonesian"); } return; }
    if(r.kind == 0 && g_vtLanguage && g_activeVt == g_vtLanguage && g_cfgLang != 0) { g_cfgLang = 0; SaveLanguageSetting(); LOGI("PC menu language: follows the game again"); }
    if(r.kind == 0) { if(MenuPending()) return; SndPlay(g_sndSelect); if(r.act) { LOGI("Page action '%s' (%p, data %d)", r.label.c_str(), (void*)r.act, r.data); r.act(g_activeTop, r.data); } }
    else if(r.kind == 1) { SndPlay(g_sndSelect); CycleToggle(r.setId); }
    else if(r.kind == 3)
    {
        if(MenuPending()) return;
        LOGI("Slot '%s' -> HandleInput", r.label.c_str());
        g_feSwallowed = 0;
        CallSelectionInput(r.item, g_activeTop);                   // the game clicks by itself when the row really acts; that click is swallowed while our own sounds are on ...
        if(g_feSwallowed) SndPlay(g_feSwallowed == 2 ? g_sndBack : g_sndSelect);   // ... and replayed here as OUR single click (a row that does nothing stays silent, like stock)
    }
}

/* A tap is a deliberate touch on one text button: down and up on the same row with the finger (nearly) still.
   TAP_SLOP is the finger wobble that is still a tap (a fraction of the screen height, about 3 mm on a phone). */
static const float TAP_SLOP = 0.045f;
static bool TapMovedAway(float x, float y) { return fabsf(x - g_pressX) > TAP_SLOP * L.H || fabsf(y - g_pressY) > TAP_SLOP * L.H; }

static void HandlePageTouch(const Touch& t)
{
    bool down = t.down; float x = (float)t.x, y = (float)t.y;
    bool inBack = g_style != PS_DIALOG && x >= L.pBackL && x <= L.pBackR && y >= L.pBackT && y <= L.pBackB;   // dialogs have no BACK button
    if(down && !g_prevDown)                       // finger went down
    {
        g_pressX = x; g_pressY = y; g_lastY = y; g_dragging = false; g_sliderRow = -1;
        if(inBack) { g_pressRow = -2; g_sel = -2; }
        else
        {
            g_pressRow = RowAtPage(y);
            if(g_pressRow >= 0)
            {
                g_sel = g_pressRow;                       // the highlight follows the finger silently: the click comes once, with the tap itself
                if(g_kind == SK_PAGE && g_pageRows[g_pressRow].kind == 2 && (IsRTL() ? x <= L.W - L.pValueR + L.pBarW + 0.04f * L.W : x >= L.pValueR - L.pBarW - 0.04f * L.W))
                { g_sliderRow = g_pressRow; SliderSetFromX(g_sliderRow, x); }
            }
        }
    }
    else if(down && g_prevDown)                   // finger is moving
    {
        if(g_sliderRow >= 0) SliderSetFromX(g_sliderRow, x);
        else if(g_pressRow != -2)
        {
            if(!g_dragging && (fabsf(y - g_pressY) > 0.02f * L.H || fabsf(x - g_pressX) > TAP_SLOP * L.H)) g_dragging = true;   // moved = not a tap
            if(g_dragging) { g_scroll -= (y - g_lastY); g_scroll = std::min(std::max(g_scroll, 0.0f), PageMaxScroll()); }
        }
        g_lastY = y;
    }
    else if(!down && g_prevDown)                  // finger lifted
    {
        if(g_sliderRow >= 0) g_sliderRow = -1;
        else if(!g_dragging)
        {
            if(g_pressRow == -2) { if(inBack) DoBack(true); }
            else if(g_pressRow >= 0 && RowAtPage(y) == g_pressRow) ActivatePageRow(g_pressRow);
        }
        g_pressRow = -1; g_dragging = false;
        if(g_sel == -2) g_sel = 0;
    }
    g_prevDown = down;
}

static void ActivateModRow(int r);   // pcextra.h
static void HandleRootTouch(const Touch& t)
{
    bool down = t.down;
    float x = (float)t.x, y = (float)t.y;

    if(g_view == VIEW_MAP) { g_prevDown = down; return; }   // the stock map and its own Back button handle the finger; we notice when it leaves the map mode (Update hook)

    bool mods = (g_view == VIEW_MODS);
    if(down && !g_prevDown)                       // finger went down
    {
        g_pressX = x; g_pressY = y; g_lastY = y; g_dragging = false;
        g_pressRow = mods ? RowAtMods(y) : RowAtMain(y);
        if(g_pressRow >= 0) g_sel = g_pressRow;   // (silent: the click comes once, with the tap itself)
    }
    else if(down && g_prevDown)                   // finger is moving
    {
        if(mods && g_pressRow != (int)g_modRows.size())
        {
            if(!g_dragging && (fabsf(y - g_pressY) > 0.02f * L.H || fabsf(x - g_pressX) > TAP_SLOP * L.H)) g_dragging = true;   // moved = not a tap
            if(g_dragging) { g_scroll -= (y - g_lastY); g_scroll = std::min(std::max(g_scroll, 0.0f), MaxScroll()); }
        }
        else
        {                                         // (main view, or the BACK row of the Mods view) no slide select: the finger never carries the highlight (or the press) to another row;
            if(g_pressRow >= 0 && TapMovedAway(x, y)) g_pressRow = -3;   // a press that wanders off is no tap at all and does nothing when lifted
        }
        g_lastY = y;
    }
    else if(!down && g_prevDown)                  // finger lifted
    {
        if(!g_dragging && g_pressRow >= 0)
        {
            if(!mods) { if(RowAtMain(y) == g_pressRow) ActivateMain(g_pressRow); }
            else
            {
                int r = RowAtMods(y);
                if(r == g_pressRow) ActivateModRow(r);
            }
        }
        g_pressRow = -1; g_dragging = false;
    }
    g_prevDown = down;
}

static void HandleTouch(const Touch& t)
{
    ComputeLayout();
    if(t.down && !g_prevDown && !g_touchLogged) { g_touchLogged = true; LOGI("First touch at %d,%d (screen %.0fx%.0f)", t.x, t.y, L.W, L.H); }
    if(g_nav != NAV_NONE || (g_returnToMods && g_kind == SK_OPTIONS)) { g_prevDown = t.down; return; }   // busy navigating: ignore taps
    if(g_kind == SK_ROOT) HandleRootTouch(t); else HandlePageTouch(t);
}

/* ------------------------------------------------------------------------------------------ */
/*  Rendering                                                                                   */
/* ------------------------------------------------------------------------------------------ */
enum { BD_WIDE = 0, BD_BIG, BD_SMALL };

static void DrawCursor(float textLeft, float textW, float rowCenterY)      // the arrow sits before the text: left of it, right of it (mirrored) when the text reads right to left
{
    if(!g_texArrow) return;
    bool rtl = IsRTL();
    float x0, x1;
    if(rtl) { x0 = textLeft + textW + L.curGap; x1 = x0 + L.curW; } else { x1 = textLeft - L.curGap; x0 = x1 - L.curW; }
    float y0 = rowCenterY - L.curH * 0.5f + 0.006f * L.H, y1 = y0 + L.curH;
    DrawTex(g_texArrow, x0, y0, x1, y1, rtl ? 62.0f / 64.0f : 0.0f, 2.0f / 128.0f, rtl ? 0.0f : 62.0f / 64.0f, 95.0f / 128.0f, COL_WHITE);
}

/* full-screen backdrop anchored to the top-right corner of the screen (16:9 / 20:9 phones) */
static void DrawBackdrop(const char* title, const Theme& th, void* bg, int style)
{
    if(!TransparentBackdrop()) FillRect(0, 0, L.W, L.H, COL_BLACK);
    if(bg && TransparentPicture())
    {
        float w, h, y0 = 0.0f;
        if(style == BD_WIDE)     { w = 0.461f * L.W; h = 0.47f * L.H; }
        else if(style == BD_BIG) { w = 0.27f * L.W;  h = 0.55f * L.H; }
        else                     { w = 0.19f * L.W;  h = 0.30f * L.H; y0 = 0.035f * L.H; }
        Col tint = (style == BD_SMALL) ? Col{ 128, 128, 128, 255 } : COL_WHITE;        // page pictures are dimmed so the value column stays readable
        DrawTex(bg, L.W - w, y0, L.W, y0 + h, 0, 0, 1, 1, tint);
    }
    float capT = g_beck.capH * L.titleScale;
    DrawText(g_beck, title, 0.05f * L.W, 0.0771f * L.H + capT, L.titleScale, 0, th.title);
}

static void DrawScrollBar(float top, float bottom, float maxScroll, Col thumb, Col track)
{
    if(maxScroll <= 0) return;
    float trackH = bottom - top, thumbH = std::max(0.04f * L.H, trackH * trackH / (trackH + maxScroll));
    float ty = top + (trackH - thumbH) * (g_scroll / maxScroll), bx = IsRTL() ? 0.02f * L.W : L.W - 0.02f * L.W;
    FillRect(bx, top, bx + 0.004f * L.H, bottom, track);
    FillRect(bx, ty, bx + 0.004f * L.H, ty + thumbH, thumb);
}

static void DrawToast(const Theme& th)
{
    if(g_toastFrames <= 0 || g_toast.empty()) return;
    float hs = (0.016f * L.H) / (float)g_bank.capH;
    DrawText(g_bank, Fit(g_bank, g_toast, hs, L.track, 0.70f * L.W), 0.06f * L.W, 0.962f * L.H, hs, L.track, th.sel);
}

static void DrawLoading()
{
    if(!TransparentBackdrop()) FillRect(0, 0, L.W, L.H, COL_BLACK);
    float cap = g_bank.capH * L.itemScale;
    std::string t = Tr("PLEASE WAIT");
    float w = g_bank.Width(t, L.itemScale, L.track);
    DrawText(g_bank, t, L.cx - w * 0.5f, L.H * 0.5f + cap * 0.5f, L.itemScale, L.track, TH_BLUE.dim);
}

static void DrawMainView()
{
    DrawBackdrop(TitleOf(g_isPause ? "PAUSE MENU" : "MAIN MENU").c_str(), TH_BLUE, g_texBack, BD_WIDE);
    float cap = g_bank.capH * L.itemScale;
    for(size_t i = 0; i < g_entries.size(); ++i)
    {
        float cy = L.firstY + L.rowStep * (float)i;
        float w = g_bank.Width(g_entries[i].label, L.itemScale, L.track);
        float left = L.cx - w * 0.5f;
        bool sel = ((int)i == g_sel);
        DrawText(g_bank, g_entries[i].label, left, cy + cap * 0.5f, L.itemScale, L.track, sel ? TH_BLUE.sel : TH_BLUE.dim);
        if(sel) DrawCursor(left, w, cy);
    }
    DrawToast(TH_BLUE);
}

static void DrawModsView()
{
    const Theme& th = TH_BLUE;
    DrawBackdrop(TitleOf("MODS").c_str(), th, g_texBack, BD_WIDE);
    float cap = g_bank.capH * L.itemScale;
    int nFiles = 0; for(size_t i = 0; i < g_modRows.size(); ++i) if(g_modRows[i].kind == MR_FILE) ++nFiles;
    char hdr[96]; snprintf(hdr, sizeof(hdr), "%d %s", nFiles, Tr(nFiles == 1 ? "MOD FOUND" : "MODS FOUND").c_str());
    float hs = (0.016f * L.H) / (float)g_bank.capH;
    DrawText(g_bank, hdr, 0.052f * L.W, 0.225f * L.H, hs, L.track, th.dim);

    float maxW = 0.62f * L.W;
    if(g_modRows.empty())
    {
        std::string t = Tr("NO MODS FOUND");
        float w = g_bank.Width(t, L.itemScale, L.track);
        DrawText(g_bank, t, L.cx - w * 0.5f, L.listTop + L.rowStep * 0.5f + cap * 0.5f, L.itemScale, L.track, th.dim);
    }
    for(size_t i = 0; i < g_modRows.size(); ++i)
    {
        float cy = L.listTop + L.rowStep * ((float)i + 0.5f) - g_scroll;
        if(cy - L.rowStep * 0.5f < L.listTop - 1 || cy + L.rowStep * 0.5f > L.listBottom + 1) continue;
        std::string t = Fit(g_bank, g_modRows[i].label, L.itemScale, L.track, maxW);
        float w = g_bank.Width(t, L.itemScale, L.track), left = L.cx - w * 0.5f;
        bool sel = ((int)i == g_sel);
        DrawText(g_bank, t, left, cy + cap * 0.5f, L.itemScale, L.track, sel ? th.sel : th.dim);
        if(sel) DrawCursor(left, w, cy);
    }
    DrawScrollBar(L.listTop, L.listBottom, MaxScroll(), th.sel, Col{ 30, 26, 6, 255 });
    if(g_sel >= 0 && g_sel < (int)g_modRows.size())
    {
        std::string f = Fit(g_bank, Upper(g_modRows[g_sel].file), hs, L.track, 0.5f * L.W);
        DrawText(g_bank, f, 0.052f * L.W, 0.962f * L.H, hs, L.track, th.dim);
    }
    {
        std::string t = Tr("BACK");
        float w = g_bank.Width(t, L.itemScale, L.track), left = L.cx - w * 0.5f;
        bool sel = (g_sel == (int)g_modRows.size());
        DrawText(g_bank, t, left, L.backY + cap * 0.5f, L.itemScale, L.track, sel ? th.sel : th.dim);
        if(sel) DrawCursor(left, w, L.backY);
    }
    DrawToast(th);
}

static void DrawBackButton(const Theme& th)                  // bottom right, like the PC options screens
{
    float sc = L.pItemScale, cap = g_bank.capH * sc;
    std::string back = Tr("BACK");
    float w = g_bank.Width(back, sc, L.track);
    DrawText(g_bank, back, L.W - 0.055f * L.W - w, 0.93f * L.H + cap * 0.5f, sc, L.track, g_sel == -2 ? th.sel : th.dim);
}

/* PC style Options list: the tiles of the stock Options screen, centered left of the picture */
static void DrawOptionsPage()
{
    const Theme& th = TH_BLUE;
    bool leftAligned = (g_style == PS_TILES_LEFT);                  // Start Game: "Game" title, picture top right, list at the left edge (like the PC menu)
    if(leftAligned) DrawBackdrop(TitleOf("GAME").c_str(), th, g_texBg[5] ? g_texBg[5] : g_texBack, BD_SMALL);
    else            DrawBackdrop(OptionsTitle().c_str(), th, g_texBg[3] ? g_texBg[3] : g_texBack, BD_BIG);
    float cap = g_bank.capH * L.itemScale;
    float top = PageWinTop(), bottom = PageWinBottom(), step = PageStep();
    if(g_optRows.empty())
    {
        DrawText(g_bank, Tr("NOTHING TO SHOW HERE"), L.pOptCx - 0.1f * L.W, top + step * 0.5f + cap * 0.5f, L.itemScale, L.track, th.dim);
    }
    for(size_t i = 0; i < g_optRows.size(); ++i)
    {
        float cy = PageRowCY((int)i);
        if(cy - step * 0.5f < top - 1 || cy + step * 0.5f > bottom + 1) continue;
        std::string t = Fit(g_bank, g_optRows[i].label, L.itemScale, L.track, 0.52f * L.W);
        float w = g_bank.Width(t, L.itemScale, L.track), left = leftAligned ? (IsRTL() ? L.W - L.sLabelX - w : L.sLabelX) : L.pOptCx - w * 0.5f;
        bool sel = ((int)i == g_sel);
        DrawText(g_bank, t, left, cy + cap * 0.5f, L.itemScale, L.track, sel ? th.sel : th.dim);
        if(sel) DrawCursor(left, w, cy);
    }
    DrawScrollBar(top, bottom, PageMaxScroll(), th.sel, Col{ 30, 34, 40, 255 });
    DrawBackButton(th);
    DrawToast(th);
}

static void* PageBg(const std::string& title)                 // PC menus use a different picture per page
{
    std::string n; for(size_t i = 0; i < title.size(); ++i) { char c = title[i]; if(c >= 'A' && c <= 'Z') c += 32; n += c; }
    int idx = 3;
    if(g_pageBg >= 0) idx = g_pageBg;
    else if(n.find("audio") != std::string::npos || n.find("sound") != std::string::npos) idx = 5;
    else if(n.find("display") != std::string::npos || n.find("graphic") != std::string::npos) idx = 3;
    else if(n.find("control") != std::string::npos) idx = 2;
    else if(n.find("game") != std::string::npos || n.find("language") != std::string::npos) idx = 4;
    else if(n.find("mod") != std::string::npos) idx = 2;
    return g_texBg[idx] ? g_texBg[idx] : g_texBack;
}

static void* SlotBg()                                          // Load / Save / Delete: one of the PC menu pictures
{
    std::string t = Upper(g_slotTitle); int idx = 2;
    if(t.find("DELETE") != std::string::npos) idx = 4; else if(t.find("SAVE") != std::string::npos) idx = 3;
    return g_texBg[idx] ? g_texBg[idx] : g_texBack;
}

static void DrawTicks(float xr, float cy, float frac, Col on, Col off, bool mirror = false)   // PC style slider: 22 growing ticks (mirrored: growing from right to left)
{
    const int N = 22;
    float total = L.pBarW, tw = total / ((float)N + (float)(N - 1) * 0.7f), gap = tw * 0.7f, hMax = 0.036f * L.H;
    float x0 = xr - total, bottom = cy + hMax * 0.5f;
    for(int i = 0; i < N; ++i)
    {
        float h = hMax * (0.30f + 0.70f * (float)i / (float)(N - 1)), x = mirror ? xr - tw - (float)i * (tw + gap) : x0 + (float)i * (tw + gap);
        bool filled = ((float)i + 0.5f) / (float)N <= frac + 0.0001f || (frac >= 0.999f);
        FillRect(x, bottom - h, x + tw, bottom, filled ? on : off);
    }
}

/* PC style page for a stock SelectScreen (Game, Controls, Audio, Display, Language, Mods settings) */
static void DrawSettingsPage()
{
    const Theme& th = TH_BLUE;
    bool slots = (g_style == PS_SLOTS);
    if(slots) DrawBackdrop(TitleOf(Upper(g_slotTitle).c_str()).c_str(), th, SlotBg(), BD_SMALL);
    else      DrawBackdrop(g_pageTitle.c_str(), th, PageBg(g_pageTitle), BD_SMALL);
    float labelX = slots ? L.sLabelX : L.pLabelX;
    float sc = L.pItemScale, cap = g_bank.capH * sc;
    float top = PageWinTop(), bottom = PageWinBottom(), step = PageStep();
    if(g_pageRows.empty())
    {
        std::string t = Tr("NOTHING TO SHOW HERE");
        DrawText(g_bank, t, L.pLabelX, top + step * 0.5f + cap * 0.5f, sc, L.track, th.dim);
    }
    float labelMax = slots ? 0.62f * L.W : 0.46f * L.W, valueMax = 0.30f * L.W;
    const bool rtl = IsRTL();
    for(size_t i = 0; i < g_pageRows.size(); ++i)
    {
        float cy = PageRowCY((int)i);
        if(cy - step * 0.5f < top - 1 || cy + step * 0.5f > bottom + 1) continue;
        PRow& r = g_pageRows[i];
        bool sel = ((int)i == g_sel);
        Col c = sel ? th.sel : th.dim;
        if(!sel && r.kind == 3 && r.label.find("CLOUD") != std::string::npos) c = Col{ 70, 76, 84, 255 };   // cloud slots that cannot be used: dimmer than the rest
        std::string lt = Fit(g_bank, r.label, sc, L.track, labelMax);
        float lw = g_bank.Width(lt, sc, L.track), lx = rtl ? L.W - labelX - lw : labelX;      // right to left: labels sit at the right edge, values / bars at the left
        DrawText(g_bank, lt, lx, cy + cap * 0.5f, sc, L.track, c);
        if(sel) DrawCursor(lx, lw, cy);
        if(r.kind == 1)
        {
            SView v;
            if(ReadSetting(r.setId, v))
            {
                if(v.value != r.lastVal) { r.lastVal = v.value; r.valText = ToggleText(v); }
                std::string t = Fit(g_bank, r.valText, sc, L.track, valueMax);
                float w = g_bank.Width(t, sc, L.track);
                DrawText(g_bank, t, rtl ? L.W - L.pValueR : L.pValueR - w, cy + cap * 0.5f, sc, L.track, c);
            }
        }
        else if(r.kind == 4)
        {
            if(g_cfgLang == r.data && (r.data != LG_AR || g_arOK))
            {
                std::string t = Fit(g_bank, Tr("ACTIVE"), sc, L.track, valueMax);
                float w = g_bank.Width(t, sc, L.track);
                DrawText(g_bank, t, rtl ? L.W - L.pValueR : L.pValueR - w, cy + cap * 0.5f, sc, L.track, c);
            }
        }
        else if(r.kind == 2)
        {
            SView v;
            if(ReadSetting(r.setId, v))
            {
                float range = (float)((long)v.vmax - (long)v.vmin);
                float f = range > 0 ? (float)(v.value - v.vmin) / range : 0.0f; f = f < 0 ? 0 : (f > 1 ? 1 : f);
                float barL = rtl ? L.W - L.pValueR : L.pValueR - L.pBarW;
                DrawTicks(barL + L.pBarW, cy, f, th.sel, Col{ 44, 50, 58, 255 }, rtl);
                char pc[16]; snprintf(pc, sizeof(pc), "%d%%", (int)(f * 100.0f + 0.5f));
                float w = g_bank.Width(pc, sc, L.track);
                DrawText(g_bank, pc, rtl ? barL + L.pBarW + 0.015f * L.W : barL - 0.015f * L.W - w, cy + cap * 0.5f, sc, L.track, c);
            }
        }
    }
    DrawScrollBar(top, bottom, PageMaxScroll(), th.sel, Col{ 30, 34, 40, 255 });
    DrawBackButton(th);
    DrawToast(th);
}

/* Yes/No and OK dialogs: the question in the middle of a black screen, buttons below it */
static void DrawDialogPage()
{
    const Theme& th = TH_BLUE;
    if(!TransparentBackdrop()) FillRect(0, 0, L.W, L.H, COL_BLACK);
    float cap = g_bank.capH * L.itemScale;
    for(size_t i = 0; i < g_dlgLines.size(); ++i)
    {
        float w = g_bank.Width(g_dlgLines[i], L.itemScale, L.track);
        DrawText(g_bank, g_dlgLines[i], L.cx - w * 0.5f, g_dlgMsgTop + g_dlgLineH * ((float)i + 0.5f) + cap * 0.5f, L.itemScale, L.track, th.title);
    }
    for(size_t i = 0; i < g_pageRows.size(); ++i)
    {
        float cy = PageRowCY((int)i);
        float w = g_bank.Width(g_pageRows[i].label, L.itemScale, L.track), left = L.cx - w * 0.5f;
        bool sel = ((int)i == g_sel);
        DrawText(g_bank, g_pageRows[i].label, left, cy + cap * 0.5f, L.itemScale, L.track, sel ? th.sel : th.dim);
        if(sel) DrawCursor(left, w, cy);
    }
    DrawToast(th);
}

#include "pcextra.h"

/* ------------------------------------------------------------------------------------------ */
/*  Resource loading (first frame, render thread)                                               */
/* ------------------------------------------------------------------------------------------ */
static void LoadResources()
{
    if(g_resTried) return;
    g_resTried = true;
    bool ok = g_bank.Load("bank") && g_beck.Load("beck");
    { bool a = g_bank.LoadArabic("bank_ar"), b = g_beck.LoadArabic("beck_ar");                      // optional: Arabic glyphs
      g_arOK = ok && a && b; if(!g_arOK) g_bank.hasAr = g_beck.hasAr = false;
      LOGI(g_arOK ? "Arabic glyph atlases loaded (bank_ar, beck_ar): Arabic is available on the Language page" : "Arabic glyph atlases (bank_ar / beck_ar in %spc/fonts/) not found: Arabic is not offered", g_dirPC.c_str()); }
    g_texArrow = LoadPNG("pc/images/arrow.png");
    g_texBack  = LoadPNG("pc/images/" + g_cfgBackground);
    for(int n = 2; n <= 5; ++n) { char pth[48]; snprintf(pth, sizeof(pth), "pc/images/back%d.png", n); g_texBg[n] = LoadPNG(pth); }
    if(!ok) LOGE("Font atlases are missing in %spc/fonts/ (bank.png/.txt, beck.png/.txt). Run tools/gen_fonts.py or copy the PCMenu folder again.", g_dirPC.c_str());
    if(!g_texArrow) LOGE("Cannot load %spc/images/arrow.png", g_dirPC.c_str());
    if(!g_texBack)  LOGE("Cannot load background %spc/images/%s (continuing without it)", g_dirPC.c_str(), g_cfgBackground.c_str());
    g_resOK = ok;                                  // cursor / backdrop are optional, fonts are not
    if(g_cfgSounds)
    {
        bool a = SndLoad(g_sndSelect, g_dirPC + "select_menu.wav"), b = SndLoad(g_sndBack, g_dirPC + "back_menu.wav");
        LOGI("Sounds: select=%d back=%d", (int)a, (int)b);
    }
    LOGI("PCMenu resources: %s", g_resOK ? "OK" : "FAILED (stock menu stays active)");
}

/* ------------------------------------------------------------------------------------------ */
/*  Hooks                                                                                       */
/* ------------------------------------------------------------------------------------------ */
struct FlagGuard { bool& f; explicit FlagGuard(bool& r) : f(r) { f = true; } ~FlagGuard() { f = false; } };

/* The PC menu itself: painted over everything the stock menu drew this frame. */
static void PaintPC(int kind, void* top)
{
    g_paintPending = false;
    if(g_toastFrames > 0) --g_toastFrames;
    ComputeLayout();
    if(!g_touchLogged) LOGI("Screen size used for layout: %.0f x %.0f", L.W, L.H);
    SetStates();
    if(g_nav != NAV_NONE || (g_returnToMods && kind == SK_OPTIONS)) DrawLoading();
    else if(kind == SK_ROOT)
    {
        BuildEntries();
        if(g_view == VIEW_MAIN)      DrawMainView();
        else if(g_view == VIEW_MODS) DrawModsView();
        // VIEW_MAP: nothing of ours is drawn - the stock map with its own Back button (bottom right) is all there is
    }
    else if(kind == SK_OPTIONS || kind == SK_START) DrawOptionsPage();
    else
    {
        RefreshPageRows(top);
        if(g_style == PS_SLOTS && (g_slotDirty || ++g_slotTick % 30 == 0)) RefreshSlotLabels(top);
        if(g_style == PS_DIALOG) DrawDialogPage(); else DrawSettingsPage();
    }
    fnRenderState(RS_ZTEST, (void*)1);
    fnRenderState(RS_ZWRITE, (void*)1);
}

DECL_HOOKv(CWidget_DrawTitleSafeArea, void)
{
    if(g_inRender && g_paintPending)
    {
        static bool once = false; if(!once) { once = true; LOGI("PC menu is painted from the end of the stock menu pass (other mods' overlays stay above it)"); }
        PaintPC(g_paintKind, g_paintTop);
    }
    CWidget_DrawTitleSafeArea();
}

DECL_HOOKv(MobileMenu_Render, void* self)
{
    FlagGuard inRender(g_inRender);
    { static bool once = false; if(!once) { once = true; LOGI("MobileMenu::Render hook is running"); } }
    if(!g_resTried) LoadResources();
    void* top = NULL;
    int kind = EvalTakeover(self, &top);
    g_kind = kind; if(kind != SK_NONE) g_liveMs = NowMs();
    if(kind != SK_NONE) { static bool seen[8] = { false }; if(!seen[kind]) { seen[kind] = true; LOGI("PC menu active: %s", kind == SK_ROOT ? (g_isPause ? "pause menu" : "title menu") : kind == SK_OPTIONS ? "options" : kind == SK_START ? "start game" : g_style == PS_SLOTS ? "save slots" : g_style == PS_DIALOG ? "dialog" : "settings page"); } }
    if(kind != SK_NONE && (top != g_activeTop || g_evalVt != g_activeVt))
    {
        g_activeTop = top; g_activeVt = g_evalVt; g_sel = 0; g_scroll = 0; g_prevDown = false; g_dragging = false; g_sliderRow = -1;
        if(kind == SK_ROOT)
        {
            g_view = VIEW_MAIN;
            if(g_pendingRootView == VIEW_MODS)                 // coming back from a mod's settings page: reopen the Mods list where it was
            {
                ComputeLayout();
                OpenView(VIEW_MODS);
                g_sel = std::min(g_modsSelSaved, (int)g_modRows.size()); g_scroll = std::min(g_modsScrollSaved, MaxScroll());
            }
            g_pendingRootView = -1; g_returnToMods = false; g_autoBack = false;
        }
        else if(kind == SK_PAGE) { g_pageRowsTop = NULL; }
        LOGI("Screen changed: kind %d, style %d, top %p, class %p", kind, g_style, top, (void*)g_evalVt);
    }
    if(kind == SK_NONE)
    {
        SetStockInert(false, NULL);
        if(!g_editorUp) { g_view = VIEW_MAIN; g_activeTop = NULL; g_activeVt = 0; }   // (behind the editor the same page is still the active one: it keeps its row / scroll position)
    }
    else                SetStockInert(kind == SK_ROOT && g_view == VIEW_MAP, top);

    EnforceMapMode(self, kind);
    g_suppressFlow = (kind == SK_ROOT); g_suppressTop = top;  // FlowScreen::Render (stock buttons) is skipped while we own the root screen
    g_paintKind = kind; g_paintTop = top; g_paintPending = (kind != SK_NONE);   // painted from the DrawTitleSafeArea hook: the last thing the stock pass does
    MobileMenu_Render(self);                       // (other mods that draw after the stock menu - an overlay icon, say - therefore end up above the PC menu)
    g_suppressFlow = false;
    if(kind == SK_NONE) { g_paintPending = false; return; }
    if(g_paintPending) PaintPC(kind, top);         // the stock pass did not reach its end (or the tail hook is not installed): paint now, like before
}

/* The game's own menu clicks: CAudioEngine::ReportFrontendAudioEvent(event 1 = select, 2 = back, 3 / 4 = screen change).
   While the PC menu is up it plays ITS click for every tap, so a game click on top of it is a second (third, fourth) one for the same tap:
   the stock code clicks when a tile callback runs, when a screen is popped, when a slot row is activated and when a screen is entered.
   They are swallowed here, and only while the PC menu owns the screen and its own sounds are loaded and on. The map is the exception: the stock map
   and its Back button (bottom right) handle the finger there, so their click is the only one and passes. Outside the PC menu nothing is touched.
   Everything that does get through is debounced like our own clicks (the same event twice within CLICK_GAP_MS is one event). */
static bool g_feHook = false;
static int64_t g_feLastMs[5] = { -100000, -100000, -100000, -100000, -100000 };
static int g_feLogged = 0, g_feDropped = 0;
static bool OwnClicks() { return g_kind != SK_NONE && g_cfgSounds && g_sndSelect.ok && g_sndBack.ok && !(g_kind == SK_ROOT && g_view == VIEW_MAP); }
DECL_HOOKv(CAudioEngine_ReportFrontendAudioEvent, void* self, int ev, float a, float b)
{
    int64_t now = NowMs();
    if(ev >= 1 && ev <= 4 && g_kind != SK_NONE && now - g_liveMs < 500)     // (the sound the game makes when the pause menu OPENS comes before the first PC frame: it is left alone)
    {
        bool own = OwnClicks(), dup = !own && now - g_feLastMs[ev] < CLICK_GAP_MS;
        if(g_feLogged < 24) { ++g_feLogged; LOGI("game click %d while the PC menu is up (kind %d, view %d, %s update): %s", ev, g_kind, g_view, g_inStockUpdate ? "inside the stock" : "outside the stock", own ? "swallowed (the PC menu clicks itself)" : dup ? "dropped (repeat)" : "passed"); }
        if(own || dup) { ++g_feDropped; if(own && (ev == 1 || ev == 2)) g_feSwallowed = ev; return; }
        g_feLastMs[ev] = now;
    }
    if(CAudioEngine_ReportFrontendAudioEvent) CAudioEngine_ReportFrontendAudioEvent(self, ev, a, b);
}

DECL_HOOKb(BackPressed_hook, void)
{
    bool r = BackPressed_hook();
    if(g_inStockUpdate && (g_kind == SK_OPTIONS || g_kind == SK_PAGE || g_kind == SK_START || (g_kind == SK_ROOT && g_view == VIEW_MODS)))
    {                                              // our UI owns Back here: remember a real key press, never let the stock code act on it
        if(r) g_backRequested = true;
        return false;
    }
    return r;
}

DECL_HOOKv(FlowScreen_Render, void* self, int a)
{
    if(g_suppressFlow && self == g_suppressTop) return;
    FlowScreen_Render(self, a);
}

DECL_HOOKb(CPad_GetEscapeJustDown, void* self)
{
    bool r = CPad_GetEscapeJustDown(self);
    if(g_inStockUpdate && (g_kind != SK_ROOT || g_view != VIEW_MAIN))   // Android back key = "BACK" in every PC screen except the root
    {
        if(r) g_backRequested = true;
        return false;
    }
    return r;
}

DECL_HOOKv(MobileMenu_Update, void* self)
{
    { static bool once = false; if(!once) { once = true; LOGI("MobileMenu::Update hook is running"); } }
    void* top = NULL;
    int kind = EvalTakeover(self, &top);
    g_kind = kind;
    bool active = (kind != SK_NONE);
    if(active) g_liveMs = NowMs();
    Touch touch = { false, 0, 0 };
    if(active)
    {
        ComputeLayout();
        ReadTouch(touch);                          // real finger state, before the stock menu is blinded
        MaskPointers();
    }
    EnforceMapMode(self, kind);
    g_inStockUpdate = active;
    MobileMenu_Update(self);
    g_inStockUpdate = false;
    if(active) UnmaskPointers();

    void* top2 = NULL;
    int kind2 = active ? EvalTakeover(self, &top2) : SK_NONE;
    if(active && kind2 != SK_NONE && top2 == g_activeTop && g_evalVt == g_activeVt)
    {
        g_kind = kind2;
        if(kind2 == SK_ROOT) BuildEntries();
        if(kind2 == SK_PAGE) RefreshPageRows(top2);
        if(g_returnToMods && g_nav == NAV_NONE && kind2 == SK_OPTIONS)  // back from a mod's page: also leave the Options screen
        {
            if(!g_autoBack && !MenuPending()) g_autoBack = DoBack(false);   // ONE Back, and only when nothing is pending
            g_prevDown = false; g_backRequested = false;
            return;
        }
        if(kind2 == SK_ROOT && g_view == VIEW_MAP)                      // the stock Back button (bottom right) switches the map mode off by itself
        {
            uint8_t mv = 1;
            if(SafeCopy((char*)self + OFF_MM_MAPMODE, &mv, 1) && mv == 0) { g_view = VIEW_MAIN; g_prevDown = false; LOGI("Map left through the stock Back: PC main view is back"); }
        }
        ControllerNav();                                                // gamepad / keyboard (1.2.0)
        if(g_backRequested)
        {
            g_backRequested = false;
            if(g_nav == NAV_NONE)
            {
                if(kind2 == SK_ROOT) { SndPlay(g_sndBack); if(g_view == VIEW_MODS) OpenView(VIEW_MAIN); else g_view = VIEW_MAIN; }
                else DoBack(true);
            }
            g_prevDown = false;
        }
        RunNav(kind2, top2);
        HandleTouch(touch);
    }
    else
    {
        g_prevDown = false; g_backRequested = false;
    }
}

/* Hooks are installed on pointer slots (GOT/PLT entries and one vtable entry), never by patching code:
   nothing is copied out of a function prologue, so Thumb IT blocks / PC-relative loads cannot break it.
   Every slot is checked against the real symbol address first; if anything differs we install nothing. */
struct SlotHook { const char* what; uintptr_t slot, expected; void* hook; void** orig; };
static std::string OwnerOf(uintptr_t addr)                  // the library an address lives in ("libZyzyGFX64.so"), for the log
{
    Dl_info di; memset(&di, 0, sizeof(di));
    if(addr && dladdr((void*)addr, &di) && di.dli_fname) { const char* b = strrchr(di.dli_fname, '/'); return b ? b + 1 : di.dli_fname; }
    return "unknown";
}
static bool SlotOK(const SlotHook& h)
{
    uintptr_t cur = 0;
    if(!h.slot || !h.expected || !SafeCopy((void*)h.slot, &cur, sizeof(cur)) || !cur) { LOGE("%s: slot %p unreadable / empty", h.what, (void*)h.slot); return false; }
    if(cur == h.expected) LOGI("%s: slot %p is untouched (-> %p)", h.what, (void*)h.slot, (void*)cur);
    else                  LOGI("%s: slot %p already hooked by another mod: %s (-> %p, original %p): chaining after it", h.what, (void*)h.slot, OwnerOf(cur).c_str(), (void*)cur, (void*)h.expected);
    return true;                                   // the build itself is verified by the symbol fingerprint in OnModLoad
}

/* ------------------------------------------------------------------------------------------ */
/*  Entry point                                                                                 */
/* ------------------------------------------------------------------------------------------ */
#define RESOLVE(var, sym) do { SET_TO(var, aml->GetSym(g_hGame, sym)); if(!(var)) { LOGE("Missing symbol: %s", sym); missing = true; } } while(0)

extern "C" void OnModLoad()
{
#ifdef PCMENU_TRANSPARENT
    logger->SetTag(BYBIT("PCMenuT32", "PCMenuT64"));
#else
    logger->SetTag(BYBIT("PCMenu32", "PCMenu64"));
#endif
    g_hGame = aml->GetLibHandle("libGTASA.so");
    if(!g_hGame) { LOGE("libGTASA.so not found. PCMenu stays disabled."); return; }

    std::string data = aml->GetAndroidDataPath();             // .../Android/data/<pkg>/files/
    if(data.empty()) { LOGE("AML returned no data path."); return; }
    if(data.back() != '/') data += '/';
    LOGI("PCMenu 1.2.1%s (%s) starting; data dir %s", kTransparentBuild ? " Transparent" : "", BYBIT("32-bit", "64-bit"), data.c_str());
    g_dirPC   = data + "PCMenu/";
    {
        std::string root = data;                                // strip ".../files/" -> ".../<package>/"
        if(root.size() > 6 && root.compare(root.size() - 6, 6, "files/") == 0) root.erase(root.size() - 6);
        g_dirMods = root + "mods/";
    }
    LoadConfig();
    if(!g_cfgEnabled) { LOGI("Disabled in pcmenu.ini"); return; }
    if(!FileExists(g_dirPC + "pc/fonts/bank.png")) { LOGE("PCMenu folder not found at %s (copy the PCMenu folder there). PCMenu stays disabled.", g_dirPC.c_str()); return; }

    bool missing = false;
    void* fn = NULL; (void)fn;
    RESOLVE(fnDrawSprite,     "_ZN10MobileMenu10DrawSpriteEP9RwTextureP9CVector2DS3_P5CRGBA");
    RESOLVE(fnDrawQuad,       "_ZN10MobileMenu8DrawQuadEP9CVector2DP5CRGBA");
    RESOLVE(fnRenderState,    "_Z16RwRenderStateSet13RwRenderStatePv");
    RESOLVE(fnPtrCoords,      "_Z25LIB_PointerGetCoordinatesiPiS_Pf");
    RESOLVE(fnPtrButton,      "_Z20LIB_PointerGetButtonii");
    g_pointers = (uint8_t*)aml->GetSym(g_hGame, "pointers");
    if(!g_pointers) { LOGE("Missing symbol: pointers"); missing = true; }
    g_libBase = aml->GetLib("libGTASA.so");
    if(!g_libBase) { LOGE("Cannot get libGTASA base"); missing = true; }
    RESOLVE(fnPngRead,        "_Z14RtPNGImageReadPKc");
    RESOLVE(fnFindRasterFmt,  "_Z23RwImageFindRasterFormatP7RwImageiPiS1_S1_S1_");
    RESOLVE(fnRasterCreate,   "_Z14RwRasterCreateiiii");
    RESOLVE(fnRasterFromImg,  "_Z20RwRasterSetFromImageP8RwRasterP7RwImage");
    RESOLVE(fnTexCreate,      "_Z15RwTextureCreateP8RwRaster");
    RESOLVE(fnImgDestroy,     "_Z14RwImageDestroyP7RwImage");
    RESOLVE(fnOnResume,       "_ZN14MainMenuScreen8OnResumeEv");
    RESOLVE(fnOnStartGame,    "_ZN14MainMenuScreen11OnStartGameEv");
    RESOLVE(fnOnStats,        "_ZN10FlowScreen7OnStatsEv");
    RESOLVE(fnOnBriefs,       "_ZN10FlowScreen8OnBriefsEv");
    RESOLVE(fnOnSettings,     "_ZN14MainMenuScreen10OnSettingsEv");
    RESOLVE(fnOnExit,         "_ZN14MainMenuScreen6OnExitEv");
    SET_TO(fnIsPause,         aml->GetSym(g_hGame, "_ZN10FlowScreen13IsPauseScreenEv"));
    uintptr_t vt = aml->GetSym(g_hGame, "_ZTV14MainMenuScreen");
    g_pRsGlobal = (int*)aml->GetSym(g_hGame, "RsGlobal");
    uintptr_t symRender = aml->GetSym(g_hGame, "_ZN10MobileMenu6RenderEv");
    uintptr_t symUpdate = aml->GetSym(g_hGame, "_ZN10MobileMenu6UpdateEv");
    uintptr_t symFlowRender = aml->GetSym(g_hGame, "_ZN10FlowScreen6RenderEi");
    uintptr_t symEscape = aml->GetSym(g_hGame, "_ZN4CPad17GetEscapeJustDownEv");
    if(!vt || !g_pRsGlobal || !symRender || !symUpdate || !symFlowRender || !symEscape) { LOGE("Missing core symbols (vtable/RsGlobal/MobileMenu/Pointer)."); missing = true; }
    if(missing) { LOGE("This libGTASA.so is not the expected build. PCMenu stays disabled."); return; }
    g_vtMainMenu = vt + 2 * sizeof(void*);
    LOGI("libGTASA base %p | pointers %p | vtable %p | render %p update %p escape %p flowRender %p", (void*)g_libBase, (void*)g_pointers, (void*)vt, (void*)symRender, (void*)symUpdate, (void*)symEscape, (void*)symFlowRender);
    #define FPEQ(sym, fp) ((((sym) - g_libBase) | 1) == (((uintptr_t)(fp)) | 1))      /* ignore the Thumb marker bit */
    if(!FPEQ(symRender, FP_RENDER) || !FPEQ(symUpdate, FP_UPDATE) || !FPEQ(symEscape, FP_ESCAPE) ||
       !FPEQ(symFlowRender, FP_FLOWRENDER) || (vt - g_libBase) != (uintptr_t)FP_VTABLE)
    {
        LOGE("libGTASA is not the build PCMenu was made for (symbol offsets differ). PCMenu stays disabled; nothing was hooked.");
        return;
    }

    // ---- PC options pages (optional: if anything is missing here the stock Options screens stay) ----
    {
        fnTextGet  = (TextGetFn)(uintptr_t)aml->GetSym(g_hGame, "_ZN5CText3GetEPKc");
        g_pTheText = (void*)aml->GetSym(g_hGame, "TheText");
        uintptr_t vA = aml->GetSym(g_hGame, "_ZTVN12SelectScreen15ActionSelectionE");
        uintptr_t vS = aml->GetSym(g_hGame, "_ZTVN12SelectScreen16SettingSelectionE");
        uintptr_t vO = aml->GetSym(g_hGame, "_ZTV14SettingsScreen");
        g_vtAction = vA ? vA + 2 * sizeof(void*) : 0; g_vtSetting = vS ? vS + 2 * sizeof(void*) : 0; g_vtSettings = vO ? vO + 2 * sizeof(void*) : 0;
        struct PageVt { const char* sym; int style; const char* title; };
        static const PageVt pageVts[] = {
            { "_ZTV12SelectScreen", PS_SETTINGS, "" }, { "_ZTV10GameScreen", PS_SETTINGS, "" }, { "_ZTV14ControlsScreen", PS_SETTINGS, "" },
            { "_ZTV13DisplayScreen", PS_SETTINGS, "" }, { "_ZTV11AudioScreen", PS_SETTINGS, "" }, { "_ZTV14LanguageScreen", PS_SETTINGS, "" },
            { "_ZTV16CharSelectScreen", PS_SETTINGS, "" },
            { "_ZTV10LoadScreen", PS_SLOTS, "Load Game" }, { "_ZTV10SaveScreen", PS_SLOTS, "Save Game" },
            { "_ZTV12DeleteScreen", PS_SLOTS, "Delete Game" }, { "_ZTV16SaveSelectScreen", PS_SLOTS, "Load Game" },
            { "_ZTV11YesNoScreen", PS_DIALOG, "" }, { "_ZTV8OkScreen", PS_DIALOG, "" },
        };
        g_selVtCount = 0;
        for(size_t i = 0; i < sizeof(pageVts) / sizeof(pageVts[0]); ++i)
        {
            uintptr_t v = aml->GetSym(g_hGame, pageVts[i].sym);
            if(v && g_selVtCount < 24) { g_selVts[g_selVtCount] = v + 2 * sizeof(void*); g_selStyle[g_selVtCount] = pageVts[i].style; g_selTitle[g_selVtCount] = pageVts[i].title; ++g_selVtCount; }
            else LOGI("Page screen class %s not found", pageVts[i].sym);
        }
        static const char* slotSyms[] = { "_ZTVN10LoadScreen19LoadScreenSelectionE", "_ZTVN10SaveScreen19SaveScreenSelectionE", "_ZTVN12DeleteScreen21DeleteScreenSelectionE" };
        g_slotSelCount = 0;
        for(size_t i = 0; i < sizeof(slotSyms) / sizeof(slotSyms[0]); ++i)
        {
            uintptr_t v = aml->GetSym(g_hGame, slotSyms[i]);
            if(v) g_slotSelVts[g_slotSelCount++] = v + 2 * sizeof(void*); else LOGI("Slot row class %s not found", slotSyms[i]);
        }
        { uintptr_t v = aml->GetSym(g_hGame, "_ZTV15StartGameScreen"); g_vtStartGame = v ? v + 2 * sizeof(void*) : 0; if(!v) LOGI("StartGameScreen class not found: Start Game stays stock"); }
        g_pLastDevice = (int*)aml->GetSym(g_hGame, "lastDevice");
        g_pFrontEnd   = (uint8_t*)aml->GetSym(g_hGame, "FrontEndMenuManager");
        { uintptr_t v = aml->GetSym(g_hGame, "_ZTV14LanguageScreen"); g_vtLanguage = v ? v + 2 * sizeof(void*) : 0; }
        LOGI("Game language byte: %d (0 English, 1 French, 2 German, 3 Italian, 4 Spanish, 5 Russian, 6 Japanese); PC menu language %d%s", GameLanguage(), UiLang(), g_cfgLang == LG_ID ? " (Indonesian, from pcmenu.ini)" : g_cfgLang == LG_AR ? (g_arOK ? " (Arabic, from pcmenu.ini)" : " (Arabic is set in pcmenu.ini but the Arabic glyph atlases are missing: ignored)") : "");
        g_ppAdjHUD    = (void**)aml->GetSym(g_hGame, "_ZN14CAdjustableHUD11m_pInstanceE");
        g_pGangWars   = (uint8_t*)aml->GetSym(g_hGame, "_ZN9CGangWars15bGangWarsActiveE");
        g_pNavZones   = (uint16_t*)aml->GetSym(g_hGame, "_ZN9CTheZones28TotalNumberOfNavigationZonesE");
        if(!g_ppAdjHUD) LOGE("CAdjustableHUD::m_pInstance not found: the button position editor could not be told apart from the Controls page, so the stock Options screens stay");
        g_gotSettings = g_libBase + GOT_SETTINGS;
        fnSetupLayout = (void (*)())aml->GetSym(g_hGame, "_ZN15CTouchInterface18SetupLayoutObjectsEv");
        fnApplyAudio  = (void (*)())aml->GetSym(g_hGame, "_Z23Menu_ApplyAudioSettingsv");
        fnRetune      = (void (*)(void*, signed char))aml->GetSym(g_hGame, "_ZN12CAudioEngine11RetuneRadioEa");
        fnSetHRTF     = (void (*)(void*, int))aml->GetSym(g_hGame, "_ZN16CAEAudioHardware7SetHRTFEi");
        fnIsPlaying   = (bool (*)(bool))aml->GetSym(g_hGame, "_Z18Menu_IsPlayingGameb");
        g_pAudioEngine = (void*)aml->GetSym(g_hGame, "AudioEngine");
        g_pAEHw        = (void*)aml->GetSym(g_hGame, "AEAudioHardware");
        uintptr_t cur = 0, orig = aml->GetSym(g_hGame, "_ZN14MobileSettings8settingsE");
        bool gotOK = SafeCopy((void*)g_gotSettings, &cur, sizeof(cur)) && cur;
        LOGI("settings table: GOT slot %p holds %p (game's own array %p)%s", (void*)g_gotSettings, (void*)cur, (void*)orig, cur != orig ? " - redirected by another mod, following it" : "");
        g_pcOptions = g_cfgPCOptions && fnTextGet && g_pTheText && g_vtAction && g_vtSetting && g_vtSettings && g_selVtCount > 0 && gotOK && g_ppAdjHUD;
        LOGI("PC options pages: %s (text %p, vtables action %p setting %p options %p start %p, %d page classes, %d slot classes, lastDevice %p, side effects: layout %p audio %p radio %p hrtf %p)",
             g_pcOptions ? "ENABLED" : "disabled", (void*)fnTextGet, (void*)g_vtAction, (void*)g_vtSetting, (void*)g_vtSettings, (void*)g_vtStartGame, g_selVtCount, g_slotSelCount, (void*)g_pLastDevice,
             (void*)fnSetupLayout, (void*)fnApplyAudio, (void*)fnRetune, (void*)fnSetHRTF);
    }
    SlotHook hooks[4] = {
        { "MobileMenu::Render",        g_libBase + GOT_RENDER, symRender,     (void*)&HookOf_MobileMenu_Render,       (void**)&MobileMenu_Render },
        { "MobileMenu::Update",        g_libBase + GOT_UPDATE, symUpdate,     (void*)&HookOf_MobileMenu_Update,       (void**)&MobileMenu_Update },
        { "CPad::GetEscapeJustDown",   g_libBase + GOT_ESCAPE, symEscape,     (void*)&HookOf_CPad_GetEscapeJustDown,  (void**)&CPad_GetEscapeJustDown },
        { "FlowScreen::Render (vtable)", vt + 5 * sizeof(void*), symFlowRender, (void*)&HookOf_FlowScreen_Render,     (void**)&FlowScreen_Render },
    };
    for(int i = 0; i < 4; ++i) if(!SlotOK(hooks[i])) { LOGE("This libGTASA.so is not the expected build. PCMenu stays disabled (nothing was hooked)."); return; }
    {   // optional second layer: the stock BackPressed(); without it the off-screen pointer coordinates still keep the stock Back quiet
        uintptr_t symBack = aml->GetSym(g_hGame, "_Z11BackPressedv");
        SlotHook hb = { "BackPressed", g_libBase + GOT_BACKPRESSED, symBack, (void*)&HookOf_BackPressed_hook, (void**)&BackPressed_hook };
        if(symBack && FPEQ(symBack, FP_BACKPRESSED) && SlotOK(hb)) { aml->HookPLT((void*)hb.slot, hb.hook, hb.orig); LOGI("hook BackPressed -> installed"); }
        else LOGI("hook BackPressed skipped (not needed for the fix, only a second safety layer)");
    }
    {   // optional: the game's own menu clicks (see the hook): without it the PC menu still works, the game's clicks just may sound on top of ours
        uintptr_t symSnd = aml->GetSym(g_hGame, "_ZN12CAudioEngine24ReportFrontendAudioEventEiff");
        SlotHook hs = { "CAudioEngine::ReportFrontendAudioEvent", g_libBase + GOT_REPORTFE, symSnd, (void*)&HookOf_CAudioEngine_ReportFrontendAudioEvent, (void**)&CAudioEngine_ReportFrontendAudioEvent };
        if(symSnd && FPEQ(symSnd, FP_REPORTFE) && SlotOK(hs)) { g_feHook = aml->HookPLT((void*)hs.slot, hs.hook, hs.orig); LOGI("hook ReportFrontendAudioEvent -> %s (one click per tap)", g_feHook ? "installed" : "FAILED"); }
        else LOGI("hook ReportFrontendAudioEvent skipped: the game's own clicks are left alone");
    }
    {   // optional: the last call of the stock menu pass. The PC menu is painted from there, so every mod that draws after the stock menu (an overlay icon, say) ends up above it
        uintptr_t symTail = aml->GetSym(g_hGame, "_ZN7CWidget17DrawTitleSafeAreaEv");
        SlotHook ht = { "CWidget::DrawTitleSafeArea", g_libBase + GOT_TITLESAFE, symTail, (void*)&HookOf_CWidget_DrawTitleSafeArea, (void**)&CWidget_DrawTitleSafeArea };
        if(symTail && FPEQ(symTail, FP_TITLESAFE) && SlotOK(ht)) { aml->HookPLT((void*)ht.slot, ht.hook, ht.orig); LOGI("hook CWidget::DrawTitleSafeArea -> installed (previous %p)", ht.orig ? *ht.orig : NULL); }
        else LOGI("hook CWidget::DrawTitleSafeArea skipped: the PC menu is painted right after the stock menu instead");
    }
    for(int i = 0; i < 4; ++i)
    {
        bool ok = aml->HookPLT((void*)hooks[i].slot, hooks[i].hook, hooks[i].orig);
        uintptr_t now = 0; SafeCopy((void*)hooks[i].slot, &now, sizeof(now));
        LOGI("hook %s -> %s (slot now %p, previous %p)", hooks[i].what, ok ? "installed" : "FAILED", (void*)now, hooks[i].orig ? *hooks[i].orig : NULL);
    }
    {   // optional: Indonesian game texts (subtitles, HUD, menus) from INDONESIAN.GXT, answered by a hook on CText::Get. Without the file nothing changes
        uintptr_t symGet = aml->GetSym(g_hGame, "_ZN5CText3GetEPKc");
        SET_TO(fnMissionName, aml->GetSym(g_hGame, "_ZN5CText26GetNameOfLoadedMissionTextEPc"));
        SlotHook hg = { "CText::Get", g_libBase + GOT_TEXTGET, symGet, (void*)&HookOf_CText_Get, (void**)&TextGet_orig };
        if(!g_cfgIdGxt) LOGI("Indonesian game texts are switched off (IndonesianText=0)");
        else if(symGet && fnMissionName && FPEQ(symGet, FP_TEXTGET) && SlotOK(hg))
        {
            bool ok = aml->HookPLT((void*)hg.slot, hg.hook, hg.orig);
            LOGI("hook CText::Get -> %s (previous %p); only acts while the PC menu language is Indonesian and INDONESIAN.GXT exists", ok ? "installed" : "FAILED", TextGet_orig);
            if(ok && g_cfgLang == LG_ID) IdGxtEnsure();                                      // load now instead of in the middle of a frame
            if(!ok) TextGet_orig = NULL;
        }
        else LOGI("hook CText::Get skipped (symbol / fingerprint differ): the game's texts stay in the game's language");
    }
    InstallExtras();                                               // transparent backdrop, controller, loading screen (1.2.0)
    LOGI("PCMenu loaded. Data: %s  Mods: %s", g_dirPC.c_str(), g_dirMods.c_str());
}
