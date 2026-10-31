/* PCMenu 1.2.0 additions, included by main.cpp after the menu model / drawing helpers:
 *   - PLT slot finder (any exported libGTASA function called through its own .got.plt)
 *   - Transparent variant (PCMENU_TRANSPARENT, libPCMenuT32/64.so): the blurred game frame behind the pause menu,
 *     the Android port of III.VC.SA.TransparentMenu (capture the frame buffer from CHud::Draw while no menu is up)
 *   - Controller support: CHID menu mappings drive the PC menu (up / down / left / right / accept / back)
 *   - Loading screen: mobilesc0..mobilesc27 splash cycle and a PC style progress bar (thin grey bar under the logo, as on PC)
 */
#pragma once
#include <link.h>
#include <elf.h>
#include <pthread.h>

/* ------------------------------------------------------------------------------------------ */
/*  Config (read by LoadConfig through ExtraConfig)                                             */
/* ------------------------------------------------------------------------------------------ */
static bool  g_cfgController   = true;    // Controller=1: gamepad / keyboard drives the PC menu
static bool  g_cfgInlineHook   = false;   // InlineHookFallback=1: hook the function body when no PLT slot is found
static bool  g_cfgTransparent  = true;    // Transparent=1 (only in the T build): the paused game shows through the menu
static bool  g_cfgTrBlur       = true;    // TransparentBlur=1
static int   g_cfgTrDim        = 150;     // TransparentDim=0..255: darkening over the game frame
static int   g_cfgTrInterval   = 2;       // CaptureInterval=N: capture every N-th game frame
static bool  g_cfgTrFlip       = false;   // TransparentFlipY=1 if the frame appears upside down on a device
static bool  g_cfgLoadScreen   = true;    // LoadingScreen=1: PC style loading screen
static int   g_cfgSplashFirst  = 0;       // SplashFirst / SplashLast: mobilesc<N>.png range
static int   g_cfgSplashLast   = 27;
static float g_cfgSplashSec    = 5.0f;    // SplashSeconds
static bool  g_cfgSplashRandom = false;   // Order=Random | Sequential
static bool  g_cfgProgressBar  = true;    // ProgressBar=1
static bool  g_cfgLoadMusic    = false;   // LoadingMusic=1: ldsc.mp3 while loading
static int   g_cfgSplashPreload = 3;      // SplashPreload=1..8: pictures prepared (main thread) per loading screen
static int   g_cfgSplashFit    = 2;       // SplashFit=Fill | Fit | PC (PC: the picture framed in black, as the PC game shows it)

static void ExtraConfig(const std::string& k, const std::string& v)
{
    int i = atoi(v.c_str());
    if(k == "Controller")              g_cfgController = i != 0;
    else if(k == "InlineHookFallback") g_cfgInlineHook = i != 0;
    else if(k == "Transparent")        g_cfgTransparent = i != 0;
    else if(k == "TransparentBlur")    g_cfgTrBlur = i != 0;
    else if(k == "TransparentDim")     g_cfgTrDim = std::min(255, std::max(0, i));
    else if(k == "CaptureInterval")    g_cfgTrInterval = std::min(30, std::max(1, i));
    else if(k == "TransparentFlipY")   g_cfgTrFlip = i != 0;
    else if(k == "LoadingScreen")      g_cfgLoadScreen = i != 0;
    else if(k == "SplashFirst")        g_cfgSplashFirst = std::min(99, std::max(0, i));
    else if(k == "SplashLast")         g_cfgSplashLast = std::min(99, std::max(0, i));
    else if(k == "SplashSeconds")      { float f = (float)atof(v.c_str()); g_cfgSplashSec = (f >= 0.5f && f <= 120.0f) ? f : 5.0f; }
    else if(k == "Order")              { std::string u = Upper(v); g_cfgSplashRandom = (u == "RANDOM" || u == "1"); }
    else if(k == "ProgressBar")        g_cfgProgressBar = i != 0;
    else if(k == "LoadingMusic")       g_cfgLoadMusic = i != 0;
    else if(k == "SplashPreload")      g_cfgSplashPreload = std::min(8, std::max(1, i));
    else if(k == "SplashFit")          { std::string u = Upper(v); g_cfgSplashFit = (u == "FILL" || u == "0") ? 0 : (u == "FIT" || u == "1") ? 1 : 2; }
}

/* ------------------------------------------------------------------------------------------ */
/*  PLT slot finder                                                                             */
/* ------------------------------------------------------------------------------------------ */
struct PltInfo { uintptr_t base; const ElfW(Sym)* symtab; const char* strtab; uintptr_t jmprel; size_t pltsz; bool rela; bool ok; };
static PltInfo g_plt = { 0, NULL, NULL, 0, 0, false, false };
static int PltPhdrCb(struct dl_phdr_info* info, size_t, void* data)
{
    PltInfo* p = (PltInfo*)data;
    if(!info->dlpi_name || !strstr(info->dlpi_name, "libGTASA.so")) return 0;
    if(g_libBase && info->dlpi_addr != g_libBase) return 0;
    uintptr_t base = info->dlpi_addr;
    for(int i = 0; i < info->dlpi_phnum; ++i)
    {
        if(info->dlpi_phdr[i].p_type != PT_DYNAMIC) continue;
        const ElfW(Dyn)* d = (const ElfW(Dyn)*)(base + info->dlpi_phdr[i].p_vaddr);
        int pltrel = 0;
        for(; d->d_tag != DT_NULL; ++d)
        {
            uintptr_t v = (uintptr_t)d->d_un.d_ptr;
            uintptr_t a = v < base ? v + base : v;          // bionic leaves d_ptr unrelocated, glibc relocates it
            switch(d->d_tag)
            {
                case DT_SYMTAB:   p->symtab = (const ElfW(Sym)*)a; break;
                case DT_STRTAB:   p->strtab = (const char*)a; break;
                case DT_JMPREL:   p->jmprel = a; break;
                case DT_PLTRELSZ: p->pltsz  = (size_t)d->d_un.d_val; break;
                case DT_PLTREL:   pltrel = (int)d->d_un.d_val; break;
                default: break;
            }
        }
        p->rela = (pltrel == DT_RELA);
        p->base = base;
        p->ok = p->symtab && p->strtab && p->jmprel && p->pltsz;
        return 1;
    }
    return 0;
}
static uintptr_t FindPltSlot(const char* name)
{
    if(!g_plt.ok) { static bool tried = false; if(tried) return 0; tried = true; dl_iterate_phdr(PltPhdrCb, &g_plt); if(!g_plt.ok) { LOGE("PLT table of libGTASA.so not found"); return 0; } }
    size_t ent = g_plt.rela ? sizeof(ElfW(Rela)) : sizeof(ElfW(Rel));
    for(size_t off = 0; off + ent <= g_plt.pltsz; off += ent)
    {
        uintptr_t r_offset; size_t symi;
        if(g_plt.rela) { const ElfW(Rela)* r = (const ElfW(Rela)*)(g_plt.jmprel + off); r_offset = r->r_offset; symi = (size_t)BYBIT(ELF32_R_SYM(r->r_info), ELF64_R_SYM(r->r_info)); }
        else           { const ElfW(Rel)*  r = (const ElfW(Rel)*) (g_plt.jmprel + off); r_offset = r->r_offset; symi = (size_t)BYBIT(ELF32_R_SYM(r->r_info), ELF64_R_SYM(r->r_info)); }
        if(!symi) continue;
        const char* n = g_plt.strtab + g_plt.symtab[symi].st_name;
        if(!strcmp(n, name)) return g_plt.base + r_offset;
    }
    return 0;
}
/* hook an exported libGTASA function through its PLT slot (inline only when allowed in the ini) */
static bool HookAny(const char* what, const char* sym, void* hook, void** orig)
{
    uintptr_t slot = FindPltSlot(sym), cur = 0;
    if(slot && SafeCopy((void*)slot, &cur, sizeof(cur)) && cur)
    {
        bool ok = aml->HookPLT((void*)slot, hook, orig);
        LOGI("hook %s -> %s (PLT slot %p, previous %p%s)", what, ok ? "installed" : "FAILED", (void*)slot, (void*)cur, OwnerOf(cur) != "libGTASA.so" ? (", owner " + OwnerOf(cur)).c_str() : "");
        return ok;
    }
    uintptr_t fn = aml->GetSym(g_hGame, sym);
    if(g_cfgInlineHook && fn)
    {
        bool ok = aml->Hook((void*)fn, hook, orig);
        LOGI("hook %s -> %s (inline at %p: InlineHookFallback=1)", what, ok ? "installed" : "FAILED", (void*)fn);
        return ok;
    }
    LOGI("hook %s skipped: no PLT slot%s", what, fn ? " (InlineHookFallback=1 would hook the function itself)" : " and no symbol");
    return false;
}

/* ------------------------------------------------------------------------------------------ */
/*  RenderWare helpers used below                                                               */
/* ------------------------------------------------------------------------------------------ */
static void* (*fnRasterPush)(void*) = NULL;
static void* (*fnRasterPop)() = NULL;
static void* (*fnRasterRenderFast)(void*, int, int) = NULL;
static void* (*fnCurCamera)() = NULL;
static int   (*fnTexDestroy)(void*) = NULL;
static void** g_pScene = NULL;                  // GlobalScene { RpWorld* world; RwCamera* camera; }
#define RWCAM_FRAMEBUFFER BYBIT(0x60, 0x80)
#define RWRAS_WIDTH       BYBIT(0x0C, 0x18)
static bool RasterSize(void* ras, int& w, int& h)
{
    int wh[2] = { 0, 0 };
    if(!ras || !SafeCopy((uint8_t*)ras + RWRAS_WIDTH, wh, sizeof(wh))) return false;
    w = wh[0]; h = wh[1];
    return w > 16 && h > 16 && w <= 16384 && h <= 16384;
}
static void* TexRaster(void* tex) { void* r = NULL; if(tex) SafeCopy(tex, &r, sizeof(r)); return r; }   // RwTexture::raster is the first field

/* ------------------------------------------------------------------------------------------ */
/*  Transparent menu (T build)                                                                  */
/* ------------------------------------------------------------------------------------------ */
#ifdef PCMENU_TRANSPARENT
static const bool kTransparentBuild = true;
#else
static const bool kTransparentBuild = false;
#endif
static void*   g_capRaster = NULL;
static void*   g_capTex = NULL;
static int     g_capW = 0, g_capH = 0, g_capFrame = 0;
static bool    g_capValid = false, g_capFailed = false, g_capHook = false;
static int64_t g_capMs = -100000;

static void* CurrentFrameBuffer()
{
    void* cam = fnCurCamera ? fnCurCamera() : NULL;
    if(!cam && g_pScene) SafeCopy(&g_pScene[1], &cam, sizeof(cam));
    void* fb = NULL;
    if(cam) SafeCopy((uint8_t*)cam + RWCAM_FRAMEBUFFER, &fb, sizeof(fb));
    return fb;
}
static void CaptureFrame()
{
    if(!kTransparentBuild || !g_cfgTransparent || g_capFailed || !fnRasterPush || !fnRasterPop || !fnRasterRenderFast) return;
    int64_t now = NowMs();
    if(now - g_liveMs < 300) return;                       // a menu is (or was just) on screen: keep the last clean game frame
    if(++g_capFrame % g_cfgTrInterval) return;
    void* fb = CurrentFrameBuffer(); int w, h;
    if(!RasterSize(fb, w, h)) return;
    if(g_pRsGlobal)                                        // sanity: the frame buffer is the screen, not some render target
    {
        int sw = 0, sh = 0; SafeCopy((uint8_t*)g_pRsGlobal + OFF_RS_WIDTH, &sw, 4); SafeCopy((uint8_t*)g_pRsGlobal + OFF_RS_HEIGHT, &sh, 4);
        if(sw > 0 && sh > 0 && (w < sw / 4 || h < sh / 4)) return;
    }
    if(!g_capRaster || w != g_capW || h != g_capH)
    {
        if(g_capTex && fnTexDestroy) fnTexDestroy(g_capTex);
        g_capTex = NULL; g_capRaster = NULL; g_capValid = false;
        g_capRaster = fnRasterCreate(w, h, 0, 0x05 /*rwRASTERTYPECAMERATEXTURE*/);
        g_capTex = g_capRaster ? fnTexCreate(g_capRaster) : NULL;
        if(!g_capTex) { g_capFailed = true; LOGE("Transparent menu: cannot create a %dx%d capture texture: the menu stays opaque", w, h); return; }
        g_capW = w; g_capH = h;
        LOGI("Transparent menu: capture texture %dx%d created", w, h);
    }
    fnRasterPush(g_capRaster);
    fnRasterRenderFast(fb, 0, 0);
    fnRasterPop();
    g_capValid = true; g_capMs = now;
}

DECL_HOOKv(CHud_Draw, void)
{
    CaptureFrame();                                        // the 3D world is finished, the HUD is not drawn yet
    CHud_Draw();
}

/* Draws the paused game (blurred and darkened) as the backdrop. false = nothing drawn: the caller paints its opaque black. */
static bool TransparentBackdrop()
{
    if(!kTransparentBuild || !g_cfgTransparent || !g_capValid || !g_capTex || !InGame()) return false;
    float v0 = g_cfgTrFlip ? 1.0f : 0.0f, v1 = g_cfgTrFlip ? 0.0f : 1.0f;
    FillRect(0, 0, L.W, L.H, COL_BLACK);
    DrawTex(g_capTex, 0, 0, L.W, L.H, 0, v0, 1, v1, COL_WHITE);
    if(g_cfgTrBlur)
    {   // 12 shifted copies, each blended with a running-average weight = a box / ring blur without shaders
        static const float ox[12] = { 1, -1, 0, 0, 0.7f, -0.7f, 0.7f, -0.7f, 2, -2, 0, 0 };
        static const float oy[12] = { 0, 0, 1, -1, 0.7f, 0.7f, -0.7f, -0.7f, 0, 0, 2, -2 };
        float r = 0.0045f * L.H;
        for(int i = 0; i < 12; ++i)
        {
            uint8_t a = (uint8_t)(255.0f / (float)(i + 2));
            float dx = ox[i] * r, dy = oy[i] * r;
            DrawTex(g_capTex, dx, dy, L.W + dx, L.H + dy, 0, v0, 1, v1, Col{ 255, 255, 255, a });
        }
    }
    FillRect(0, 0, L.W, L.H, Col{ 0, 0, 0, (uint8_t)g_cfgTrDim });
    return true;
}
static bool TransparentPicture() { return !(kTransparentBuild && g_cfgTransparent && g_capValid && InGame()); }   // the page pictures stay off over the game frame

/* ------------------------------------------------------------------------------------------ */
/*  Controller                                                                                  */
/* ------------------------------------------------------------------------------------------ */
enum { HM_ACCEPT = 0x38, HM_CANCEL = 0x39, HM_DOWN = 0x3C, HM_UP = 0x3D, HM_LEFT = 0x3E, HM_RIGHT = 0x3F, HM_MACCEPT = 0x40, HM_MBACK = 0x41, HM_MMAP = 0x42 };
static bool (*CHID_IsJustPressed)(int) = NULL;
static bool (*CHID_IsPressed)(int, float*) = NULL;
static bool (*CHID_IsReleased)(int) = NULL;
static bool (*CHID_IsPressedPos)(int) = NULL;
static bool (*CHID_IsPressedNeg)(int) = NULL;
static bool (*CHID_IsReleasedPos)(int) = NULL;
static bool (*CHID_IsReleasedNeg)(int) = NULL;
static bool (*CHID_IsDoubleTapped)(int) = NULL;
static bool g_padHooked = false, g_padUsed = false;

static bool PadOwnsMenu() { return g_cfgController && g_kind != SK_NONE && !(g_kind == SK_ROOT && g_view == VIEW_MAP) && NowMs() - g_liveMs < 500; }
static bool IsMenuMapping(int m)
{
    switch(m)
    {
        case HM_ACCEPT: case HM_CANCEL: case HM_DOWN: case HM_UP: case HM_LEFT: case HM_RIGHT: case HM_MACCEPT: case HM_MMAP: return true;
        case HM_MBACK: return !(g_kind == SK_ROOT && g_view == VIEW_MAIN);   // on the root the stock BACK (= resume) still works
        default: return false;
    }
}
static bool Blind(int m) { return g_inStockUpdate && PadOwnsMenu() && IsMenuMapping(m); }   // the stock menu must not act on the pad while ours does
DECL_HOOKb(HID_JustPressed, int m)       { return Blind(m) ? false : HID_JustPressed(m); }
DECL_HOOKb(HID_Pressed, int m, float* v) { if(Blind(m)) { if(v) *v = 0; return false; } return HID_Pressed(m, v); }
DECL_HOOKb(HID_Released, int m)          { return Blind(m) ? false : HID_Released(m); }
DECL_HOOKb(HID_PressedPos, int m)        { return Blind(m) ? false : HID_PressedPos(m); }
DECL_HOOKb(HID_PressedNeg, int m)        { return Blind(m) ? false : HID_PressedNeg(m); }
DECL_HOOKb(HID_ReleasedPos, int m)       { return Blind(m) ? false : HID_ReleasedPos(m); }
DECL_HOOKb(HID_ReleasedNeg, int m)       { return Blind(m) ? false : HID_ReleasedNeg(m); }
DECL_HOOKb(HID_DoubleTapped, int m)      { return Blind(m) ? false : HID_DoubleTapped(m); }

static bool PadJust(int m) { bool (*f)(int) = HID_JustPressed ? HID_JustPressed : CHID_IsJustPressed; return f && f(m); }
static bool PadHeld(int m) { bool (*f)(int, float*) = HID_Pressed ? HID_Pressed : CHID_IsPressed; float v = 0; return f && f(m, &v); }

struct Repeat { bool held; int64_t next; };
static Repeat g_rep[4] = { { false, 0 }, { false, 0 }, { false, 0 }, { false, 0 } };
static bool PadRepeat(int idx, int m)                      // just pressed, then auto-repeat while held (350 ms, then every 90 ms)
{
    int64_t now = NowMs();
    bool held = PadHeld(m), just = PadJust(m);
    Repeat& r = g_rep[idx];
    if(just || (held && !r.held)) { r.held = true; r.next = now + 350; return true; }
    if(!held) { r.held = false; return false; }
    if(now >= r.next) { r.next = now + 90; return true; }
    return false;
}

static void ActivateModRow(int r)
{
    if(r == (int)g_modRows.size()) { SndPlay(g_sndBack); OpenView(VIEW_MAIN); return; }
    if(r < 0 || r > (int)g_modRows.size()) return;
    ModRow& mr = g_modRows[r];
    if(mr.kind == MR_TAB && mr.cb) { SndPlay(g_sndSelect); VoidFn cb = mr.cb; LOGI("Mods action '%s' (tile %d) -> %p", mr.label.c_str(), mr.idx, (void*)cb); CallTile(g_activeTop, mr.idx, cb); }
    else if(mr.kind == MR_SETTINGS) { SndPlay(g_sndSelect); StartNav(""); }
    else if(mr.kind == MR_FILE && g_pcOptions && fnOnSettings) { SndPlay(g_sndSelect); StartNav(mr.label); }
}
static void KeepModRowVisible()
{
    if(g_sel < 0 || g_sel >= (int)g_modRows.size()) return;
    float top = (float)g_sel * L.rowStep, bot = top + L.rowStep, win = L.listBottom - L.listTop;
    if(top < g_scroll) g_scroll = top;
    if(bot > g_scroll + win) g_scroll = bot - win;
    g_scroll = std::min(std::max(g_scroll, 0.0f), MaxScroll());
}
static void KeepPageRowVisible()
{
    if(g_sel < 0 || g_sel >= PageCount()) return;
    float top = PageStep() * (float)g_sel, bot = top + PageStep(), win = PageWinBottom() - PageWinTop();
    if(top < g_scroll) g_scroll = top;
    if(bot > g_scroll + win) g_scroll = bot - win;
    g_scroll = std::min(std::max(g_scroll, 0.0f), PageMaxScroll());
}
static void PageAdjust(int dir)                            // left / right on a settings row
{
    if(g_kind != SK_PAGE || g_sel < 0 || g_sel >= (int)g_pageRows.size()) return;
    PRow& r = g_pageRows[g_sel];
    SView v; if((r.kind != 1 && r.kind != 2) || !ReadSetting(r.setId, v)) return;
    if(IsRTL()) dir = -dir;
    int nv;
    if(r.kind == 1) { nv = v.value + dir; if(nv > v.vmax) nv = v.vmin; if(nv < v.vmin) nv = v.vmax; }
    else            { int step = std::max(1, (v.vmax - v.vmin) / 20); nv = std::min(v.vmax, std::max(v.vmin, v.value + dir * step)); }
    if(WriteSetting(r.setId, nv)) SndPlay(g_sndSelect);
}

/* called from the MobileMenu::Update hook, after the stock update, while the PC menu owns the screen */
static void ControllerNav()
{
    if(!g_cfgController || !g_padHooked) return;
    if(g_nav != NAV_NONE || (g_returnToMods && g_kind == SK_OPTIONS)) return;
    if(g_kind == SK_ROOT && g_view == VIEW_MAP) return;    // the stock map reads the pad itself (its Back leaves the map mode; the Update hook notices)
    bool up = PadRepeat(0, HM_UP), down = PadRepeat(1, HM_DOWN), left = PadRepeat(2, HM_LEFT), right = PadRepeat(3, HM_RIGHT);
    bool accept = PadJust(HM_MACCEPT) || PadJust(HM_ACCEPT);
    bool back = PadJust(HM_MBACK) || PadJust(HM_CANCEL);
    if(!(up || down || left || right || accept || back)) return;
    if(!g_padUsed) { g_padUsed = true; LOGI("Controller input reached the PC menu"); }
    ComputeLayout();

    if(g_kind == SK_ROOT && g_view == VIEW_MAIN)
    {
        int n = (int)g_entries.size(); if(n <= 0) return;
        if(g_sel < 0 || g_sel >= n) g_sel = 0;
        if(up)   { g_sel = (g_sel + n - 1) % n; SndPlay(g_sndSelect); }
        if(down) { g_sel = (g_sel + 1) % n; SndPlay(g_sndSelect); }
        if(accept) ActivateMain(g_sel);
        return;                                            // (Back on the root = the stock resume / nothing: left to the game)
    }
    if(g_kind == SK_ROOT && g_view == VIEW_MODS)
    {
        int n = (int)g_modRows.size() + 1;                 // + BACK
        if(g_sel < 0 || g_sel >= n) g_sel = 0;
        if(up)   { g_sel = (g_sel + n - 1) % n; SndPlay(g_sndSelect); KeepModRowVisible(); }
        if(down) { g_sel = (g_sel + 1) % n; SndPlay(g_sndSelect); KeepModRowVisible(); }
        if(accept) ActivateModRow(g_sel);
        if(back) { SndPlay(g_sndBack); OpenView(VIEW_MAIN); }
        return;
    }
    // pages / tiles: rows 0..n-1, then BACK (-2) except in dialogs
    int n = PageCount();
    bool hasBack = (g_style != PS_DIALOG);
    if(up || down)
    {
        std::vector<int> order; for(int i = 0; i < n; ++i) order.push_back(i); if(hasBack) order.push_back(-2);
        if(order.empty()) return;
        int pos = 0; for(size_t i = 0; i < order.size(); ++i) if(order[i] == g_sel) pos = (int)i;
        int m = (int)order.size();
        pos = up ? (pos + m - 1) % m : (pos + 1) % m;
        g_sel = order[pos]; SndPlay(g_sndSelect);
        KeepPageRowVisible();
    }
    if(left || right) PageAdjust(right ? 1 : -1);
    if(accept) { if(g_sel == -2) DoBack(true); else ActivatePageRow(g_sel); }
    if(back) g_backRequested = true;                       // handled right after (same path as the Android back key)
}

/* ------------------------------------------------------------------------------------------ */
/*  Loading screen                                                                              */
/* ------------------------------------------------------------------------------------------ */
/* Threads: on Android the loading screen is drawn by the game's LOADING THREAD (CLoadingScreen::Init starts it, Shutdown stops it)
   while the main thread keeps loading the world. RenderWare objects must therefore never be created or destroyed from the
   RenderSplash hook: that raced the main thread's texture loading and froze the game at the end of a short loading screen
   (New Game / Load). All pictures are loaded on the MAIN thread in the Init hook, before the loading thread starts, and the
   render hook only draws what is ready. */
static float*   g_pLsPercent = NULL;
static uint8_t* g_pLsLegal = NULL;
static bool     g_lsHook = false, g_lsBarHook = false, g_lsInitHook = false, g_lsLogged = false;
static std::atomic<int64_t> g_lsLastMs(-100000);
static int64_t  g_lsStartMs = 0, g_lsSwitchMs = 0;
static std::vector<int> g_lsAvail;                         // mobilesc numbers whose file exists
static bool     g_lsScanned = false;
static int      g_lsSeq = 0;                               // position in g_lsAvail of the next picture to load (sequential order across loading screens)
static const int LS_MAXSET = 8;
static void*    g_lsSet[LS_MAXSET];                        // pictures of the current loading screen (main thread fills them, loading thread draws them)
static std::atomic<int> g_lsSetN(0);
static int      g_lsShown = 0;                             // index into g_lsSet (loading thread only)

static void ScanSplashes()
{
    g_lsScanned = true; g_lsAvail.clear();
    int a = std::min(g_cfgSplashFirst, g_cfgSplashLast), b = std::max(g_cfgSplashFirst, g_cfgSplashLast);
    for(int i = a; i <= b; ++i) { char p[64]; snprintf(p, sizeof(p), "pc/lds/mobilesc%d.png", i); if(FileExists(g_dirPC + p)) g_lsAvail.push_back(i); }
    LOGI("Loading screen: %d splash pictures found in %spc/lds/ (mobilesc%d..mobilesc%d)", (int)g_lsAvail.size(), g_dirPC.c_str(), a, b);
}
static void* LoadSplash(int n) { char p[64]; snprintf(p, sizeof(p), "pc/lds/mobilesc%d.png", n); return LoadPNG(p); }
static int PickNext(int cur)
{
    int n = (int)g_lsAvail.size(); if(n == 0) return -1; if(n == 1) return 0;
    if(g_cfgSplashRandom) { int r; do { r = rand() % n; } while(r == cur); return r; }
    return (cur + 1) % n;
}
static void FreeSplashSet()                                 // main thread, loading thread not running
{
    int n = g_lsSetN.exchange(0);
    for(int i = 0; i < n; ++i) { if(g_lsSet[i] && fnTexDestroy) fnTexDestroy(g_lsSet[i]); g_lsSet[i] = NULL; }
}
static void PrepareSplashSet()                              // main thread, before the loading thread starts
{
    FreeSplashSet();
    if(!g_lsScanned) ScanSplashes();
    int want = std::min(std::min(g_cfgSplashPreload, LS_MAXSET), (int)g_lsAvail.size()), n = 0, guard = 0;
    if(g_cfgSplashRandom && !g_lsAvail.empty()) g_lsSeq = rand() % (int)g_lsAvail.size();
    while(n < want && guard++ < 64 && !g_lsAvail.empty())
    {
        g_lsSeq %= (int)g_lsAvail.size();
        int num = g_lsAvail[g_lsSeq];
        void* t = LoadSplash(num);
        if(!t) { g_lsAvail.erase(g_lsAvail.begin() + g_lsSeq); continue; }   // unreadable file: never tried again
        g_lsSet[n++] = t;
        g_lsSeq = g_cfgSplashRandom ? PickNext(g_lsSeq) : g_lsSeq + 1;
    }
    g_lsShown = 0;
    g_lsSetN.store(n);
}

/* where the picture goes on screen; the bar is placed in picture coordinates, so it always sits under the logo */
struct PicRect { float x0, y0, x1, y1, u0, v0, u1, v1; };
static PicRect SplashRect(void* tex)
{
    int tw = 1024, th = 512; RasterSize(TexRaster(tex), tw, th);
    float sa = (float)tw / (float)th, da = L.W / L.H;
    PicRect r = { 0, 0, L.W, L.H, 0, 0, 1, 1 };
    if(g_cfgSplashFit == 0)                                 // Fill: cover the screen, crop the rest
    {
        if(da > sa) { float h = L.W / sa; r.y0 = (L.H - h) * 0.5f; r.y1 = r.y0 + h; }
        else        { float w = L.H * sa; r.x0 = (L.W - w) * 0.5f; r.x1 = r.x0 + w; }
    }
    else
    {
        float k = (g_cfgSplashFit == 2) ? 0.889f : 1.0f;    // PC: the picture framed in black, as the PC game shows it
        float h = L.H * k, w = h * sa;
        if(w > L.W * k) { w = L.W * k; h = w / sa; }
        r.x0 = (L.W - w) * 0.5f; r.y0 = (L.H - h) * 0.5f; r.x1 = r.x0 + w; r.y1 = r.y0 + h;
    }
    return r;
}
static void DrawSplash(void* tex, const PicRect& r, uint8_t a)
{
    // (Fill) the parts outside the screen are cut by the UV range, so nothing is drawn off screen
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1, x0 = r.x0, y0 = r.y0, x1 = r.x1, y1 = r.y1;
    if(x0 < 0) { u0 = -x0 / (r.x1 - r.x0); u1 = 1 - u0; x0 = 0; x1 = L.W; }
    if(y0 < 0) { v0 = -y0 / (r.y1 - r.y0); v1 = 1 - v0; y0 = 0; y1 = L.H; }
    DrawTex(tex, x0, y0, x1, y1, u0, v0, u1, v1, Col{ 255, 255, 255, a });
}
static float LoadPercent()
{
    float p = 0; if(g_pLsPercent) SafeCopy(g_pLsPercent, &p, sizeof(p));
    return p != p ? 0.0f : std::min(100.0f, std::max(0.0f, p));
}
/* PC loading bar (taken from the PC game): a thin bar right under the "San Andreas" logo, black outline, grey fill,
   the empty part at half brightness. No text, no background strip. Measured on the PC screenshot: picture x 8.8%..23.8%
   (here matched to the mobilesc logo: 7.5%..36.5%), top at 91.4% of the picture height, 1.9% high. */
static void DrawProgressBar(const PicRect& r, uint8_t a)
{
    float frac = LoadPercent() / 100.0f;
    float pw = r.x1 - r.x0, ph = r.y1 - r.y0;
    float x0 = r.x0 + pw * 0.091f, x1 = r.x0 + pw * 0.238f;     // measured on the PC game: just under "San Andreas"
    float y0 = r.y0 + ph * 0.897f, y1 = y0 + std::max(6.0f, ph * 0.0165f);
    if(y1 > L.H - 2) { float d = y1 - (L.H - 2); y0 -= d; y1 -= d; }   // (Fill on a tall screen) keep it on screen
    float b = std::max(1.0f, (y1 - y0) / 5.0f);                         // outline
    FillRect(x0, y0, x1, y1, Col{ 0, 0, 0, a });
    float ix0 = x0 + b, ix1 = x1 - b, iy0 = y0 + b, iy1 = y1 - b, fx = ix0 + (ix1 - ix0) * frac;
    float m = (iy0 + iy1) * 0.5f;
    if(fx > ix0)
    {   // fill with the soft vertical shading of the PC bar (90 at the edges, 106 in the middle)
        FillRect(ix0, iy0, fx, m, Col{ 96, 96, 98, a });
        FillRect(ix0, m, fx, iy1, Col{ 104, 103, 106, a });
        FillRect(ix0, iy0, fx, iy0 + b * 0.6f, Col{ 86, 86, 88, a });
    }
    if(fx < ix1) FillRect(fx, iy0, ix1, iy1, Col{ 48, 48, 49, a });
}

/* ---- loading music (ldsc.mp3 through an OpenSL URI/FD player; stopped by a watcher when the loading screen is gone) ---- */
static SLObjectItf g_musObj = NULL; static SLPlayItf g_musPlay = NULL; static int g_musFd = -1;
static std::atomic<bool> g_musOn(false);
static void MusicStop()
{
    if(!g_musOn.exchange(false)) return;
    if(g_musPlay) (*g_musPlay)->SetPlayState(g_musPlay, SL_PLAYSTATE_STOPPED);
    if(g_musObj) (*g_musObj)->Destroy(g_musObj);
    g_musObj = NULL; g_musPlay = NULL;
    if(g_musFd >= 0) close(g_musFd);
    g_musFd = -1;
}
static void* MusicWatch(void*)
{
    while(g_musOn) { usleep(100000); if(NowMs() - g_lsLastMs.load() > 1500 && NowMs() - g_lsStartMs > 3000) { MusicStop(); LOGI("Loading music stopped"); } }
    return NULL;
}
static void MusicStart()
{
    if(!g_cfgLoadMusic || g_musOn || !SndInit()) return;
    std::string p = g_dirPC + "ldsc.mp3";
    int fd = open(p.c_str(), O_RDONLY); if(fd < 0) return;
    off_t len = lseek(fd, 0, SEEK_END); lseek(fd, 0, SEEK_SET);
    SLDataLocator_AndroidFD loc = { SL_DATALOCATOR_ANDROIDFD, fd, 0, (SLAint64)len };
    SLDataFormat_MIME mime = { SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED };
    SLDataSource src = { &loc, &mime };
    SLDataLocator_OutputMix locOut = { SL_DATALOCATOR_OUTPUTMIX, g_slMixObj };
    SLDataSink snk = { &locOut, NULL };
    if((*g_slEngine)->CreateAudioPlayer(g_slEngine, &g_musObj, &src, &snk, 0, NULL, NULL) != SL_RESULT_SUCCESS ||
       (*g_musObj)->Realize(g_musObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS ||
       (*g_musObj)->GetInterface(g_musObj, SL_IID_PLAY, &g_musPlay) != SL_RESULT_SUCCESS)
    { if(g_musObj) (*g_musObj)->Destroy(g_musObj); g_musObj = NULL; g_musPlay = NULL; close(fd); LOGE("Loading music: %s not playable", p.c_str()); return; }
    g_musFd = fd;
    (*g_musPlay)->SetPlayState(g_musPlay, SL_PLAYSTATE_PLAYING);
    g_musOn = true;
    pthread_t th; if(pthread_create(&th, NULL, MusicWatch, NULL) == 0) pthread_detach(th); else MusicStop();
    LOGI("Loading music started (%s)", p.c_str());
}

static bool LegalScreen() { uint8_t b = 0; if(g_pLsLegal) SafeCopy(g_pLsLegal, &b, 1); return b != 0; }

/* main thread: the loading screen is about to start */
DECL_HOOKv(LS_Init, bool legal, bool dontReload)
{
    if(g_cfgLoadScreen && !legal && NowMs() - g_lsLastMs.load() > 500)   // (never while a loading thread may still draw the current set)
    {
        int64_t t0 = NowMs();
        PrepareSplashSet();
        g_lsStartMs = 0;                                    // the render hook restarts its fade / timer
        g_lsLastMs.store(-100000);
        if(!g_lsLogged) { g_lsLogged = true; LOGI("PC loading screen: %d picture(s) prepared on the main thread in %d ms (%s order, %.1f s each)", g_lsSetN.load(), (int)(NowMs() - t0), g_cfgSplashRandom ? "random" : "sequential", g_cfgSplashSec); }
        MusicStart();
    }
    LS_Init(legal, dontReload);
}

/* loading thread: draw only (no RenderWare object is created or destroyed here) */
DECL_HOOKv(LS_RenderSplash, void)
{
    int n = g_lsSetN.load();
    if(!g_cfgLoadScreen || LegalScreen() || n <= 0) { LS_RenderSplash(); return; }   // nothing prepared: the stock picture (and the stock bar)
    int64_t now = NowMs();
    if(g_lsStartMs == 0 || now - g_lsLastMs.load() > 1500) { g_lsStartMs = now; g_lsSwitchMs = now; g_lsShown = 0; }
    g_lsLastMs.store(now);
    ComputeLayout(); SetStates();
    const float fadeMs = 600.0f, holdMs = g_cfgSplashSec * 1000.0f;
    float t = (float)(now - g_lsSwitchMs);
    if(n > 1 && t >= holdMs) { g_lsShown = (g_lsShown + 1) % n; g_lsSwitchMs = now; t = 0; }
    void* cur = g_lsSet[g_lsShown % n];
    void* nxt = n > 1 ? g_lsSet[(g_lsShown + 1) % n] : NULL;
    FillRect(0, 0, L.W, L.H, COL_BLACK);
    float fin = std::min(1.0f, (float)(now - g_lsStartMs) / fadeMs);
    uint8_t ai = (uint8_t)(255.0f * fin);
    PicRect r = SplashRect(cur);
    DrawSplash(cur, r, ai);
    if(nxt && t > holdMs - fadeMs)
    {
        float k = std::min(1.0f, (t - (holdMs - fadeMs)) / fadeMs);
        DrawSplash(nxt, SplashRect(nxt), (uint8_t)(255.0f * k));
    }
    if(g_cfgProgressBar) DrawProgressBar(r, ai);
    fnRenderState(RS_ZTEST, (void*)1);
    fnRenderState(RS_ZWRITE, (void*)1);
}
DECL_HOOKv(LS_RenderLoadingBar, void)
{
    if(g_cfgLoadScreen && g_cfgProgressBar && !LegalScreen() && g_lsSetN.load() > 0) return;   // our bar is drawn with the splash
    LS_RenderLoadingBar();
}

/* ------------------------------------------------------------------------------------------ */
/*  Install (from OnModLoad, after the core hooks)                                              */
/* ------------------------------------------------------------------------------------------ */
static void InstallExtras()
{
    SET_TO(fnRasterPush,       aml->GetSym(g_hGame, "_Z19RwRasterPushContextP8RwRaster"));
    SET_TO(fnRasterPop,        aml->GetSym(g_hGame, "_Z18RwRasterPopContextv"));
    SET_TO(fnRasterRenderFast, aml->GetSym(g_hGame, "_Z18RwRasterRenderFastP8RwRasterii"));
    SET_TO(fnCurCamera,        aml->GetSym(g_hGame, "_Z25RwCameraGetCurrentCamerav"));
    SET_TO(fnTexDestroy,       aml->GetSym(g_hGame, "_Z16RwTextureDestroyP9RwTexture"));
    g_pScene = (void**)aml->GetSym(g_hGame, "Scene");
    {
        uintptr_t s = FindPltSlot("_ZN10MobileMenu6RenderEv");
        LOGI("PLT finder: MobileMenu::Render slot %p (known %p) %s", (void*)s, (void*)(g_libBase + GOT_RENDER), s == g_libBase + GOT_RENDER ? "OK" : "differs");
    }

    if(kTransparentBuild)
    {
        if(!g_cfgTransparent) LOGI("Transparent menu switched off (Transparent=0)");
        else if(!fnRasterPush || !fnRasterPop || !fnRasterRenderFast || (!fnCurCamera && !g_pScene)) LOGE("Transparent menu: RenderWare raster functions missing: the menu stays opaque");
        else g_capHook = HookAny("CHud::Draw (frame capture)", "_ZN4CHud4DrawEv", (void*)&HookOf_CHud_Draw, (void**)&CHud_Draw);
    }

    if(g_cfgController)
    {
        SET_TO(CHID_IsJustPressed, aml->GetSym(g_hGame, "_ZN4CHID13IsJustPressedE10HIDMapping"));
        SET_TO(CHID_IsPressed,     aml->GetSym(g_hGame, "_ZN4CHID9IsPressedE10HIDMappingPf"));
        struct H { const char* sym; void* hook; void** orig; };
        H hs[] = {
            { "_ZN4CHID13IsJustPressedE10HIDMapping",      (void*)&HookOf_HID_JustPressed,  (void**)&HID_JustPressed },
            { "_ZN4CHID9IsPressedE10HIDMappingPf",          (void*)&HookOf_HID_Pressed,      (void**)&HID_Pressed },
            { "_ZN4CHID10IsReleasedE10HIDMapping",          (void*)&HookOf_HID_Released,     (void**)&HID_Released },
            { "_ZN4CHID17IsPressedPositiveE10HIDMapping",   (void*)&HookOf_HID_PressedPos,   (void**)&HID_PressedPos },
            { "_ZN4CHID17IsPressedNegativeE10HIDMapping",   (void*)&HookOf_HID_PressedNeg,   (void**)&HID_PressedNeg },
            { "_ZN4CHID18IsReleasedPositiveE10HIDMapping",  (void*)&HookOf_HID_ReleasedPos,  (void**)&HID_ReleasedPos },
            { "_ZN4CHID18IsReleasedNegativeE10HIDMapping",  (void*)&HookOf_HID_ReleasedNeg,  (void**)&HID_ReleasedNeg },
            { "_ZN4CHID14IsDoubleTappedE10HIDMapping",      (void*)&HookOf_HID_DoubleTapped, (void**)&HID_DoubleTapped },
        };
        int n = 0;
        for(size_t i = 0; i < sizeof(hs) / sizeof(hs[0]); ++i) if(HookAny(hs[i].sym, hs[i].sym, hs[i].hook, hs[i].orig)) ++n;
        g_padHooked = (HID_JustPressed || CHID_IsJustPressed) && (HID_Pressed || CHID_IsPressed);
        LOGI("Controller: %d of 8 CHID hooks installed, PC menu navigation %s", n, g_padHooked ? "ON" : "OFF (CHID functions not found)");
    }
    else LOGI("Controller support switched off (Controller=0)");

    if(g_cfgLoadScreen)
    {
        g_pLsPercent = (float*)aml->GetSym(g_hGame, "_ZN14CLoadingScreen15m_PercentLoadedE");
        g_pLsLegal   = (uint8_t*)aml->GetSym(g_hGame, "_ZN14CLoadingScreen14m_bLegalScreenE");
        if(!g_pLsPercent) { uintptr_t p = 0; if(SafeCopy((void*)(g_libBase + BYBIT(0x677FA8, 0x84DF70)), &p, sizeof(p)) && p) g_pLsPercent = (float*)p; }
        if(!g_pLsLegal)   { uintptr_t p = 0; if(SafeCopy((void*)(g_libBase + BYBIT(0x67611C, 0x84A298)), &p, sizeof(p)) && p) g_pLsLegal = (uint8_t*)p; }
        g_lsInitHook = HookAny("CLoadingScreen::Init", "_ZN14CLoadingScreen4InitEbb", (void*)&HookOf_LS_Init, (void**)&LS_Init);
        if(g_lsInitHook)                                    // without Init (main thread) nothing can be prepared safely: the stock loading screen stays
        {
            g_lsHook = HookAny("CLoadingScreen::RenderSplash", "_ZN14CLoadingScreen12RenderSplashEv", (void*)&HookOf_LS_RenderSplash, (void**)&LS_RenderSplash);
            if(g_lsHook && g_cfgProgressBar)
                g_lsBarHook = HookAny("CLoadingScreen::RenderLoadingBar", "_ZN14CLoadingScreen16RenderLoadingBarEv", (void*)&HookOf_LS_RenderLoadingBar, (void**)&LS_RenderLoadingBar);
        }
        else LOGI("PC loading screen off: CLoadingScreen::Init could not be hooked (pictures must be loaded on the main thread)");
        LOGI("Loading screen: percent %p legal %p, init hook %d, splash hook %d, bar hook %d", (void*)g_pLsPercent, (void*)g_pLsLegal, (int)g_lsInitHook, (int)g_lsHook, (int)g_lsBarHook);
    }
    else LOGI("PC loading screen switched off (LoadingScreen=0)");
}
