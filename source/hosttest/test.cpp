// Host-side simulation: the REAL main.cpp driven by a fake game (64-bit memory layout)
#ifdef OLD_SOURCE
  #define OFF_SCR_CHOSEN 0x48
  #define PTR_X 0x0C
  #define PTR_Y 0x10
  #include OLD_SOURCE
#else
  #include "../jni/main.cpp"
#endif
#include <sys/stat.h>
Logger* logger = new Logger;

static int fails = 0, checks = 0;
#define CHECK(c) do { ++checks; if(!(c)) { ++fails; printf("  FAIL line %d: %s\n", __LINE__, #c); } else if(getenv("PCM_V")) printf("  ok: %s\n", #c); } while(0)

/* ---------------- fake game ---------------- */
#ifndef OLD_SOURCE
static int64_t fakeNow = 5000000; static int64_t fClock() { return fakeNow; }              // the mod's clock (a frame = 16 ms)
static int engEv[8];                                                                      // clicks that really reached the game's audio engine, per event id
static void fEngine(void*, int ev, float, float) { if(ev >= 0 && ev < 8) ++engEv[ev]; }
static void StockClick(int ev) { HookOf_CAudioEngine_ReportFrontendAudioEvent(NULL, ev, 1.0f, 1.0f); }   // the stock menu code asking for a frontend sound
static int sndSel = 0, sndBack = 0;                                                       // our own clips that were enqueued
static SLresult fEnqSel(SLAndroidSimpleBufferQueueItf, const void*, SLuint32) { ++sndSel; return 0; }
static SLresult fEnqBack(SLAndroidSimpleBufferQueueItf, const void*, SLuint32) { ++sndBack; return 0; }
static SLresult fClr(SLAndroidSimpleBufferQueueItf) { return 0; }
static const SLAndroidSimpleBufferQueueItf_ bqSelVt = { fEnqSel, fClr }, bqBackVt = { fEnqBack, fClr };
static const SLAndroidSimpleBufferQueueItf_* bqSelP = &bqSelVt; static const SLAndroidSimpleBufferQueueItf_* bqBackP = &bqBackVt;
static bool slotSilent = false, mapBackDouble = false;
#else
static void StockClick(int) {}
static bool slotSilent = false, mapBackDouble = false;
#endif
static int fakeRs[8];
alignas(8) static uint8_t fakePointers[4 * PTR_STRIDE + 64];
static int fx = 0, fy = 0, drawSprites = 0, drawQuads = 0;
static int  fBtn(int p, int b) { if(p < 0 || p > 1) return -1; return *(int*)(fakePointers + p * PTR_STRIDE + b * 4); }
static int  fCoords(int p, int* x, int* y, float*) { *x = *(int*)(fakePointers + p * PTR_STRIDE + PTR_X); *y = *(int*)(fakePointers + p * PTR_STRIDE + PTR_Y); return 1; }
static std::string opsBuf;
static void fSprite(void* tex, V2* v, V2* uv, Col* c) { ++drawSprites; char b[400]; snprintf(b, sizeof b, "S %d %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %d %d %d %d\n", (int)(uintptr_t)tex, v[0].x, v[0].y, v[1].x, v[1].y, v[2].x, v[2].y, v[3].x, v[3].y, uv[0].x, uv[0].y, uv[1].x, uv[1].y, uv[2].x, uv[2].y, uv[3].x, uv[3].y, c[0].r, c[0].g, c[0].b, c[0].a); opsBuf += b; }
static void fQuad(V2* v, Col* c) { ++drawQuads; char b[300]; snprintf(b, sizeof b, "Q %g %g %g %g %g %g %g %g %d %d %d %d\n", v[0].x, v[0].y, v[1].x, v[1].y, v[2].x, v[2].y, v[3].x, v[3].y, c[0].r, c[0].g, c[0].b, c[0].a); opsBuf += b; }
static void* fPng(const char*) { return NULL; }
static void fRS(int, void*) {}

alignas(8) static uint8_t mmBuf[256];
static void* stackArr[8]; static int depth = 0; static void* pending = NULL; static int trans = 0; static bool menuClosed = false;
static int TRANS_FRAMES = 6;
static void Sync() { *(int*)(mmBuf + OFF_MM_SCREENCOUNT) = depth; *(void***)(mmBuf + OFF_MM_SCREENS) = stackArr; *(void**)(mmBuf + OFF_MM_PENDING) = pending; }
static void PushNow(void* s) { stackArr[depth++] = s; Sync(); }
// MobileMenu::ProcessPending: pending == the screen below the top -> pop; otherwise push it
static void fProcessPending() { if(!pending) return; if(depth >= 2 && stackArr[depth - 2] == pending) --depth; else stackArr[depth++] = pending; pending = NULL; Sync(); StockClick(3); }
// FlowScreen::OnStats & co: if(pending) ProcessPending(); pending = newScreen
static void Push(void* s) { if(pending) fProcessPending(); pending = s; trans = TRANS_FRAMES; Sync(); }
static int backCalls = 0;
// MenuScreen::OnBack -> MobileMenu::RemoveTopScreen(false,false)
static void fOnBack(void*) { ++backCalls; StockClick(2); if(depth == 0) return; if(pending) fProcessPending(); if(depth >= 2) { pending = stackArr[depth - 2]; trans = TRANS_FRAMES; } else { depth = 0; menuClosed = true; } Sync(); }

alignas(8) static uint8_t rootScr[256], optScr[256], pGame[256], pDisp[256], pAudio[256], pMods[256];
static void* vtRoot[16], *vtOpt[16], *vtPage[16], *vtAct[4], *vtSet[4], *vtOther[4];
static std::vector<std::string> calls;
static void cRes() { StockClick(1); calls.push_back("resume"); }
static void cStg() { StockClick(1); calls.push_back("start"); }
static void cSta() { StockClick(1); calls.push_back("stats"); }
static void cBri() { StockClick(1); calls.push_back("brief"); }
static void cQui() { StockClick(1); calls.push_back("quit"); }
static int tabSeenRoot = -999, tabSeenOpt = -999;
static void cTab() { StockClick(1); calls.push_back("tab"); tabSeenRoot = *(int*)(rootScr + OFF_SCR_CHOSEN); }
static void tabA() { tabSeenOpt = *(int*)(optScr + OFF_SCR_CHOSEN) * 10 + 1; }      // "A" marker
static void tabB() { tabSeenOpt = *(int*)(optScr + OFF_SCR_CHOSEN) * 10 + 2; }      // "B" marker
static int onSettingsCalls = 0;
static void cOpt() { StockClick(1); ++onSettingsCalls; Push(optScr); }
static void pushGame() { StockClick(1); Push(pGame); }
static void pushDisp() { StockClick(1); Push(pDisp); }
static void pushAudio() { StockClick(1); Push(pAudio); }
static void pushMods() { StockClick(1); Push(pMods); }
static StockItem rootItems[8], optItems[6];
static void* gameItems[8], *dispItems[8], *audioItems[8], *modsItems[12];

/* settings table behind a redirected GOT slot */
alignas(8) static uint8_t fakeSettings[40 * 40]; static uintptr_t fakeGot;
static const char* namesOnOff[] = { "MOB_OFF", "MOB_ON" };
static const char* names3[] = { "LOW", "MEDIUM", "HIGH" };
static void SetS(int id, const char** names, int val, int mn, int mx, int type)
{
    uint8_t* b = fakeSettings + id * SET_STRIDE;
    memcpy(b + SET_NAMES, &names, sizeof(names)); memcpy(b + SET_VALUE, &val, 4); memcpy(b + SET_MIN, &mn, 4); memcpy(b + SET_MAX, &mx, 4); memcpy(b + SET_TYPE, &type, 4);
}
static int audioCalls = 0, layoutCalls = 0, restoreCalls = 0, restoreData = 0; static void* restoreScr = NULL;
static void fApply() { ++audioCalls; } static void fLayout() { ++layoutCalls; }
static void fRestore(void* s, int d) { ++restoreCalls; restoreScr = s; restoreData = d; }
static void fNoop(void*, int) {}

static unsigned short gxtBuf[96], gxtMissing[2] = { '?', 0 };
static int gameLang = 0; alignas(8) static uint8_t fakeFront[0x80];                // the fake game's language = the byte at FrontEndMenuManager+0x48
struct GxtE { const char* k; const char* v; };
static const GxtE gxtEn[] = { { "FEO_GAM", "Game" }, { "FEO_DIS", "Display" }, { "FEO_AUD", "Audio" }, { "MOB_OFF", "Off" }, { "MOB_ON", "On" }, { "MOB_SFX", "SFX" }, { "FEP_RES", "Resume" },
    { "FES_NEW", "New Game" }, { "FES_LOA", "Load Game" }, { "FES_DEL", "Delete Game" }, { "FEM_YES", "Yes" }, { "FEM_NO", "No" }, { "FESZ_QL", "Are you sure you want to load this game?" },
    { "FEP_STG", "Start Game" }, { "FEP_OPT", "Options" }, { "FEP_QUI", "Quit Game" }, { "FEH_STA", "Stats" }, { "FEH_BRI", "Brief" }, { "FEH_MAP", "Map" }, { "FEO_LAN", "Language" },
    { "FEL_ENG", "English" }, { "FEL_FRE", "Francais" }, { "FEL_RUS", "Russian" },
    { "FEH_LOA", "Game" }, { "FEC_RED", "Controls" }, { "FED_BRI", "Brightness" }, { "FEX_UNK", "Frobnicate" }, { "FEL_GER", "German" }, { "FEL_ITA", "Italian" }, { "FEL_SPA", "Spanish" }, { "FEL_JPN", "Japanese" }, { "MOB_RTD", "Restore Defaults" } };
static const GxtE gxtFr[] = { { "FEP_RES", "Reprendre" }, { "FEP_STG", "Commencer le jeu" }, { "FEH_MAP", "Carte" }, { "FEH_STA", "Statistiques" }, { "FEH_BRI", "Briefing" }, { "FEP_OPT", "Options" },
    { "FEP_QUI", "Quitter le jeu" }, { "FEO_GAM", "Jeu" }, { "FEO_DIS", "Affichage" }, { "FEO_AUD", "Audio" }, { "FEO_LAN", "Langue" }, { "MOB_ON", "Activ\x9e" }, { "MOB_OFF", "D\x9esactiv\x9e" },
    { "FES_NEW", "Nouvelle partie" }, { "FES_LOA", "Charger une partie" }, { "FES_DEL", "Supprimer une partie" }, { "FEH_LOA", "Jeu" } };
static const GxtE gxtDe[] = { { "FEP_OPT", "Optionen" }, { "FEO_AUD", "Audio" }, { "FEO_GAM", "Spiel" }, { "FEO_DIS", "Anzeige" } };
static unsigned short* fText(void*, const char* key)
{
    const GxtE* t = gxtEn; size_t n = sizeof(gxtEn) / sizeof(gxtEn[0]);
    if(gameLang == 1) { t = gxtFr; n = sizeof(gxtFr) / sizeof(gxtFr[0]); }
    else if(gameLang == 2) { t = gxtDe; n = sizeof(gxtDe) / sizeof(gxtDe[0]); }
    for(size_t k = 0; k < n; ++k) if(!strcmp(t[k].k, key))
    {
        const char* v = t[k].v; int i = 0;
        if(gameLang == 5 || gameLang == 6) { unsigned b0 = gameLang == 5 ? 0xC0 : 0x3042; gxtBuf[0] = b0; gxtBuf[1] = b0 + 1; gxtBuf[2] = b0 + 2; gxtBuf[3] = 0; return gxtBuf; }        // Cyrillic-like codes: the font has no glyphs for them
        for(; v[i]; ++i) gxtBuf[i] = (unsigned char)v[i];
        gxtBuf[i] = 0; return gxtBuf;
    }
    if(gameLang == 5 || gameLang == 6) for(size_t k = 0; k < sizeof(gxtEn) / sizeof(gxtEn[0]); ++k) if(!strcmp(gxtEn[k].k, key)) { gxtBuf[0] = 0xC0; gxtBuf[1] = 0; return gxtBuf; }
    return gxtMissing;
}

static uint8_t* MkItem(uintptr_t vt, const char* tag) { uint8_t* p = (uint8_t*)calloc(1, 64); *(uintptr_t*)p = vt; *(const char**)(p + PTR_SZ) = tag; return p; }
static void* MkAction(const char* tag, void (*act)(void*, int), int data) { uint8_t* p = MkItem(g_vtAction, tag); *(void**)(p + ITEM_ONSELECT) = (void*)act; *(int*)(p + ITEM_ACTDATA) = data; return p; }
static void* MkSetting(const char* tag, int id) { uint8_t* p = MkItem(g_vtSetting, tag); *(int*)(p + ITEM_SETID) = id; return p; }
static void SetupPage(uint8_t* scr, void** items, int n) { *(void**)scr = vtPage; *(int*)(scr + SEL_ITEMCOUNT) = n; *(void***)(scr + SEL_ITEMS) = items; }

/* ---- Start Game flow: StartGameScreen (3 tiles), Load/Delete slot lists, Yes/No dialog ---- */
alignas(8) static uint8_t startScr[256], loadScr[256], delScr[256], yesScr[256], saveScr[256], oddScr[256];
static void* vtStart[16], *vtLoadScr[16], *vtDelScr[16], *vtYesNo[16], *vtSaveScr[16], *vtSlot[16];
static StockItem startItems[3];
static void* loadItems[12], *delItems[12], *yesItems[4], *saveItems[12], *oddItems[2];
static int fakeLastDevice = 0, slotInputs = 0, lastSlotHit = -1, yesCalls = 0, noCalls = 0, newGameCalls = 0;
static unsigned short slotBuf[64];
static const char* slotNames[9] = { "AUTOSAVE: EMPTY SLOT", "BMX", "IN THE BEGINNING", "EMPTY SLOT", "EMPTY SLOT", "EMPTY SLOT", "EMPTY SLOT", "CLOUD NOT AVAILABLE", "CLOUD NOT AVAILABLE" };
static const unsigned short* fSlotTitle(void* item)                      // SaveSelection::GetTitle: builds the text in a static buffer
{
    int idx = *(int*)((char*)item + 2 * PTR_SZ); int i = 0;
    for(; slotNames[idx][i]; ++i) slotBuf[i] = (unsigned short)slotNames[idx][i]; slotBuf[i] = 0; return slotBuf;
}
static void fSlotInput(void* item, void* screen, float)                  // LoadScreenSelection::HandleInput: only acts when the pointer state is "released" (0)
{
    int st = *(int*)(mmBuf + MM_PSTATE + fakeLastDevice * 4);
    if(st != 0) return;
    ++slotInputs; lastSlotHit = *(int*)((char*)item + 2 * PTR_SZ); if(!slotSilent) StockClick(1);
    if(screen == (void*)loadScr) Push(yesScr);
}
static void yesFunc(void* scr, int) { ++yesCalls; fOnBack(scr); }
static void noFunc(void* scr, int)  { ++noCalls;  fOnBack(scr); }
static void cNewGame() { ++newGameCalls; depth = 0; pending = NULL; menuClosed = true; Sync(); }   // Menu_NewGame + MobileMenu::Unload: the whole menu goes away
static void cLoadPush() { StockClick(1); Push(loadScr); }
static void cDelPush()  { StockClick(1); Push(delScr); }
static void cStartPush() { StockClick(1); calls.push_back("startscreen"); Push(startScr); }
static void* MkSlot(int idx) { uint8_t* p = MkItem((uintptr_t)vtSlot, NULL); *(int*)(p + 2 * PTR_SZ) = idx; return p; }
static void SetupSlots(uint8_t* scr, void** vtb, void** items) { *(void**)scr = vtb; for(int i = 0; i < 9; ++i) items[i] = MkSlot(i); *(int*)(scr + SEL_ITEMCOUNT) = 9; *(void***)(scr + SEL_ITEMS) = items; }

static bool playing = true; static bool fIsPlaying(bool) { return playing; }
static bool fBackPressedOrig()
{
    uint32_t st = ((uint32_t*)fakePointers)[0]; int x = *(int*)(fakePointers + PTR_X), y = *(int*)(fakePointers + PTR_Y);
    return st == 0 && x >= (int)(0.92f * fakeRs[2]) && y >= (int)(0.91f * fakeRs[3]);
}
static bool useBackHook = false, stockBackLogic = true; static int stockBackCalls = 0;
static uint32_t seenInStock = 99; static bool fakeEscape = false;
static int topUpdates = 0;
static void* fakeAdj = NULL; static int editorUpdates = 0, editorRenders = 0; static bool editorCloseTap = false;   // CAdjustableHUD::m_pInstance and what the stock code does while it exists
static uint8_t fakeGang = 1; static uint16_t fakeZones = 0;                                                     // CGangWars::bGangWarsActive / CTheZones::TotalNumberOfNavigationZones
static void fToggleEditor(void*, int) { fakeAdj = fakeAdj ? NULL : (void*)0x1234; }                              // ControlsScreen::DoAdjustableHUD -> CAdjustableHUD::Toggle
static bool fFlowYes(void*) { return true; }
static bool fFlowNo(void*)  { return false; }
static void fStockUpdate(void*)
{
    if(fakeAdj)                                                  // MobileMenu::Update: with the editor up only CAdjustableHUD::Update runs (it reads the real pointers), then return
    {
        ++editorUpdates; seenInStock = ((uint32_t*)fakePointers)[0];
        if(editorCloseTap && seenInStock == 2) fakeAdj = NULL;   // a press on the editor's own DONE button
        return;
    }
    if(mmBuf[OFF_MM_MAPMODE] && depth > 0 && fBackPressedOrig()) { mmBuf[OFF_MM_MAPMODE] = 0; StockClick(2); if(mapBackDouble) StockClick(2); }     // the stock Back button of the map (bottom right) switches the map mode off
    if(depth > 0 && !mmBuf[OFF_MM_MAPMODE]) ++topUpdates;        // MobileMenu::Update: the top screen is only updated outside the map mode
    seenInStock = ((uint32_t*)fakePointers)[0]; HookOf_CPad_GetEscapeJustDown(NULL);
    // SelectScreen::Update / FlowScreen::Update: if(BackPressed()) OnBack();
    if(stockBackLogic && depth >= 2 && (*(void***)stackArr[depth - 1] == vtPage || *(void***)stackArr[depth - 1] == vtOpt))
    {
#ifdef OLD_SOURCE
        bool bp = fBackPressedOrig();
#else
        bool bp = useBackHook ? HookOf_BackPressed_hook() : fBackPressedOrig();
#endif
        if(bp) { ++stockBackCalls; fOnBack(stackArr[depth - 1]); }
    }
    if(pending) { if(trans > 0) --trans; else fProcessPending(); }
}
static int topRenders = 0, tailOrig = 0; static bool stockCallsTail = true; static int tailCallsPerFrame = 1;
static void fTailOrig() { ++tailOrig; }                                                       // the real CWidget::DrawTitleSafeArea
static void fStockRender(void*)                                                              // MobileMenu::Render: top screen (not in map mode), ..., RenderFontBuffer, DrawTitleSafeArea
{
    if(fakeAdj) { ++editorRenders; return; }
    if(depth > 0 && !mmBuf[OFF_MM_MAPMODE]) ++topRenders;
    if(stockCallsTail) for(int i = 0; i < tailCallsPerFrame; ++i) HookOf_CWidget_DrawTitleSafeArea();
}      // MobileMenu::Render: the top screen is only drawn outside the map mode
static void fFlowRender(void*, int) {}
static bool fEscape(void*) { return fakeEscape; }

static bool prevT = false;
static void Frame(bool down, float x, float y)
{
#ifndef OLD_SOURCE
    fakeNow += 16;
#endif
    fx = (int)x; fy = (int)y;
    if(down || (x != 0 || y != 0)) { *(int*)(fakePointers + PTR_X) = (int)x; *(int*)(fakePointers + PTR_Y) = (int)y; }   // the game keeps the LAST finger position
    ((uint32_t*)fakePointers)[0] = down ? (prevT ? 3 : 2) : 0; prevT = down;
    HookOf_MobileMenu_Update(mmBuf);
    HookOf_MobileMenu_Render(mmBuf);
}
static void Shot(const char* name)
{
    const char* dir = getenv("PCM_SHOTS"); if(!dir) return;
    opsBuf.clear(); Frame(false, 0, 0);
    std::string p = std::string(dir) + "/" + name + ".txt"; FILE* f = fopen(p.c_str(), "w"); if(f) { fputs(opsBuf.c_str(), f); fclose(f); }
}
static void Idle(int n) { for(int i = 0; i < n; ++i) Frame(false, 0, 0); }
static float StockBackX() { return L.W * 0.96f; } static float StockBackY() { return L.H * 0.95f; }   // the stock Back button of the map: bottom right
static void StockBackTap() { Frame(true, StockBackX(), StockBackY()); Frame(true, StockBackX(), StockBackY()); Frame(false, StockBackX(), StockBackY()); Idle(2); }
static void QuickTap(float x, float y) { Frame(true, x, y); Frame(false, x, y); }
static void Tap(float x, float y) { Frame(true, x, y); Frame(true, x, y); Frame(false, x, y); Idle(12); }

static float OverX() { return L.W * 0.95f; }
static float OverY() { return L.H * 0.945f; }
static float rootY(int i) { return L.firstY + L.rowStep * (float)i; }
static float modsY(int i) { return L.listTop + L.rowStep * ((float)i + 0.5f) - g_scroll; }

int main()
{
    // ---- environment
    fakeRs[2] = getenv("PCM_W") ? atoi(getenv("PCM_W")) : 1600; fakeRs[3] = getenv("PCM_H") ? atoi(getenv("PCM_H")) : 720; g_pRsGlobal = fakeRs;
    fnPngRead = fPng; g_dirPC = getenv("PCM_DIR") ? getenv("PCM_DIR") : "../../files/PCMenu/";
    g_bank.Load("bank"); g_beck.Load("beck"); g_bank.LoadArabic("bank_ar"); g_beck.LoadArabic("beck_ar"); g_bank.tex = (void*)1; g_beck.tex = (void*)2;   // real glyph metrics; textures are replayed by the preview script
    g_texArrow = (void*)3; g_texBack = (void*)4; for(int i = 2; i <= 5; ++i) g_texBg[i] = (void*)(uintptr_t)(10 + i);
    g_resTried = g_resOK = true; g_cfgSounds = false; g_cfgEnabled = true; g_cfgPCOptions = true;
    fnDrawSprite = fSprite; fnDrawQuad = fQuad; fnRenderState = fRS; fnPtrCoords = fCoords; fnPtrButton = fBtn;
    g_pointers = fakePointers; g_libBase = 0;
#ifndef OLD_SOURCE
    BackPressed_hook = fBackPressedOrig;
#endif
    MobileMenu_Update = fStockUpdate; MobileMenu_Render = fStockRender; FlowScreen_Render = fFlowRender; CPad_GetEscapeJustDown = fEscape; CWidget_DrawTitleSafeArea = fTailOrig;
    g_pFrontEnd = fakeFront;
#ifndef OLD_SOURCE
    g_clockFn = fClock; CAudioEngine_ReportFrontendAudioEvent = fEngine;
#endif
    fnOnResume = cRes; fnOnStartGame = cStg; fnOnStats = cSta; fnOnBriefs = cBri; fnOnExit = cQui; fnOnSettings = cOpt;
    for(int i = 0; i < 16; ++i) vtRoot[i] = vtOpt[i] = vtPage[i] = (void*)0x1000;
    vtRoot[9] = vtOpt[9] = vtPage[9] = (void*)fOnBack;
    vtRoot[11] = vtOpt[11] = (void*)fFlowYes; vtPage[11] = (void*)fFlowNo;        // MenuScreen::IsFlow (slot 11): FlowScreens say yes, SelectScreens no
    g_vtMainMenu = (uintptr_t)vtRoot; g_vtSettings = (uintptr_t)vtOpt; g_selVts[0] = (uintptr_t)vtPage; g_selVtCount = 1;
    g_vtAction = (uintptr_t)vtAct; g_vtSetting = (uintptr_t)vtSet;
    fnTextGet = fText; g_pTheText = (void*)0x77; g_pcOptions = true;
    fakeGot = (uintptr_t)fakeSettings; g_gotSettings = (uintptr_t)&fakeGot;
    fnApplyAudio = fApply; fnSetupLayout = fLayout; fnIsPlaying = fIsPlaying;
    g_ppAdjHUD = &fakeAdj; g_pGangWars = &fakeGang; g_pNavZones = &fakeZones;
    mkdir("/tmp/pcm_mods", 0777); for(const char* f : { "libVisualObject.so", "libRadarRotation.so", "libZeta.so", "AML" }) { std::string p = std::string("/tmp/pcm_mods/") + f; FILE* fp = fopen(p.c_str(), "w"); if(fp) fclose(fp); }
    g_dirMods = "/tmp/pcm_mods/";

    // ---- screens
    *(void**)rootScr = vtRoot; rootScr[OFF_SCR_ISPAUSE] = 1;
    rootItems[0] = { NULL, "FEP_RES", cRes }; rootItems[1] = { NULL, "FEP_STG", cStg }; rootItems[2] = { NULL, "FEH_STA", cSta };
    rootItems[3] = { NULL, "FEH_BRI", cBri }; rootItems[4] = { NULL, "FEP_OPT", cOpt }; rootItems[5] = { NULL, "FEP_QUI", cQui }; rootItems[6] = { NULL, "MyTab", cTab };
    *(int*)(rootScr + OFF_SCR_ITEMCOUNT) = 7; *(StockItem**)(rootScr + OFF_SCR_ITEMS) = rootItems;
    *(void**)optScr = vtOpt;
    optItems[0] = { NULL, "FEH_LOA", pushGame }; optItems[1] = { NULL, "FEO_DIS", pushDisp }; optItems[2] = { NULL, "FEO_AUD", pushAudio }; optItems[3] = { NULL, "Mods settings", pushMods };
    optItems[4] = { NULL, "ModTabA", tabA }; optItems[5] = { NULL, "ModTabB", tabB };
    *(int*)(optScr + OFF_SCR_ITEMCOUNT) = 6; *(StockItem**)(optScr + OFF_SCR_ITEMS) = optItems;
    SetS(3, namesOnOff, 0, 0, 1, 0); SetS(5, names3, 0, 0, 2, 0); SetS(12, NULL, 40, 0, 80, 1);
    gameItems[0] = MkSetting("Show FPS", 3); gameItems[1] = MkAction("Restore Defaults", fRestore, 7); SetupPage(pGame, gameItems, 2);
    dispItems[0] = MkSetting("Visual Effects", 5); dispItems[1] = MkSetting("MOB_SFX", 12); dispItems[2] = MkSetting("Show FPS", 3);
    dispItems[3] = MkAction("Restore Defaults", fRestore, 7); dispItems[4] = MkItem((uintptr_t)vtOther, "Radio"); SetupPage(pDisp, dispItems, 5);
    audioItems[0] = MkSetting("MOB_SFX", 12); SetupPage(pAudio, audioItems, 1);
    modsItems[0] = MkAction("SAUTILS V1.6", fNoop, 0); modsItems[1] = MkSetting("RADAR ROTATION", 3); modsItems[2] = MkSetting("SPEEDOMETER", 5);
    modsItems[3] = MkAction("VISUAL OBJECT V1.0.3", fNoop, 0); modsItems[4] = MkAction("IMGUI V.1.2.0", fNoop, 0); modsItems[5] = MkSetting("ENABLE BULLETTRACES", 3); SetupPage(pMods, modsItems, 6);

    // =================================================================================== S1 root + options
    printf("S0  v1.1.3: the MODS entry is gone from the menus by default (the Mods list lives in Options)\n");
    CHECK(g_cfgModsInMain == false);
    {   // pcmenu.ini: ShowModsInMenu=1 brings the entry back, the old ModsInMainMenu key (from earlier versions) must not
        std::string keep = g_dirPC; mkdir("/tmp/pcm_ini", 0777); g_dirPC = "/tmp/pcm_ini/";
        FILE* f = fopen("/tmp/pcm_ini/pcmenu.ini", "w"); fputs("ModsInMainMenu=1\r\nShowModsInMenu=0\r\n", f); fclose(f); LoadConfig(); CHECK(g_cfgModsInMain == false);
        f = fopen("/tmp/pcm_ini/pcmenu.ini", "w"); fputs("; comment\nShowModsInMenu=1\n", f); fclose(f); LoadConfig(); CHECK(g_cfgModsInMain == true);
        g_cfgModsInMain = false; g_dirPC = keep;
    }
    PushNow(rootScr); Idle(3);
    CHECK(g_kind == SK_ROOT && g_isPause);
    CHECK(g_entries.size() == 7);
    { bool anyMods = false; for(auto& e : g_entries) if(e.label == "MODS") anyMods = true; CHECK(!anyMods); }
    CHECK(g_entries[5].label == "OPTIONS" && g_entries[6].label == "QUIT GAME");
    g_cfgModsInMain = true;                                                  // ShowModsInMenu=1: the old behaviour is still there when asked for
    Idle(3);
    printf("S1  pause root -> Options\n");
    CHECK(g_entries.size() == 8);
    CHECK(g_entries[5].label == "OPTIONS" && g_entries[6].label == "MODS");
    CHECK(drawSprites > 0 && drawQuads > 0);
    Shot("1_pause");
    ComputeLayout();
    Frame(true, L.cx, rootY(0)); CHECK(seenInStock == 0);                 // the stock menu never sees our finger
    CHECK(((uint32_t*)fakePointers)[0] == 2);                             // ...and the real state is restored afterwards
    Frame(false, 0, 0); Idle(2);
    Tap(L.cx, rootY(5));
    CHECK(onSettingsCalls == 1);
    CHECK(g_kind == SK_OPTIONS);
    CHECK(g_optRows.size() == 6);
    Shot("2_options");
    CHECK(g_optRows.size() == 6 && g_optRows[0].title == "Game" && g_optRows[1].title == "Display" && g_optRows[3].title == "Mods settings");

    // =================================================================================== S2 settings page
    printf("S2  Options -> Display page: toggle, slider, button\n");
    ComputeLayout();
    Tap(L.pOptCx, PageRowCY(1));
    CHECK(g_pageTitle == "Display");
    CHECK(g_kind == SK_PAGE);
    CHECK(g_pageRows.size() == 4);                                         // the unknown "Radio" item is skipped
    CHECK(g_pageRows.size() == 4 && g_pageRows[0].kind == 1 && g_pageRows[1].kind == 2 && g_pageRows[3].kind == 0);
    CHECK(g_pageRows.size() == 4 && g_pageRows[0].label == "VISUAL EFFECTS" && g_pageRows[1].label == "SFX");   // GXT key resolved
    SView v;
    ComputeLayout();
    Tap(L.pLabelX + 60, PageRowCY(0));                                      // tap row 0 -> next option
    ReadSetting(5, v); CHECK(v.value == 1);
    Tap(L.pLabelX + 60, PageRowCY(0)); Tap(L.pLabelX + 60, PageRowCY(0));   // 1 -> 2 -> wraps to 0
    ReadSetting(5, v); CHECK(v.value == 0);
    Tap(L.pLabelX + 60, PageRowCY(0)); Idle(2);
    CHECK(g_pageRows[0].valText == "MEDIUM");
    WriteSetting(12, 60); Frame(false, 0, 0); g_sel = 1; Shot("3_display"); WriteSetting(12, 40);
    int a0 = audioCalls;
    float barL = L.pValueR - L.pBarW;
    Frame(true, barL + L.pBarW * 0.75f, PageRowCY(1)); Frame(true, barL + L.pBarW * 0.75f, PageRowCY(1));
    ReadSetting(12, v); CHECK(v.value >= 58 && v.value <= 62);
    CHECK(audioCalls > a0);                                                // side effect for setting 12 (audio)
    Frame(true, barL + L.pBarW * 0.25f, PageRowCY(1)); ReadSetting(12, v); CHECK(v.value >= 18 && v.value <= 22);   // drag
    Frame(true, barL - 500, PageRowCY(1)); ReadSetting(12, v); CHECK(v.value == 0);                                   // clamped
    Frame(false, 0, 0); Idle(2);
    CHECK(g_sliderRow == -1);
    Tap(L.pLabelX + 60, PageRowCY(3));
    CHECK(restoreCalls == 1 && restoreScr == (void*)pDisp && restoreData == 7);

    // =================================================================================== S3 back
    printf("S3  Back button and Android back key\n");
    Tap(L.pBackL + 30, L.pBackT + 20);
    CHECK(backCalls == 1 && depth == 2); CHECK(g_kind == SK_OPTIONS);
    Tap(L.pBackL + 30, L.pBackT + 20);
    CHECK(backCalls == 2 && depth == 1); CHECK(g_kind == SK_ROOT && g_view == VIEW_MAIN);
    Tap(L.cx, rootY(5)); ComputeLayout(); Tap(L.pOptCx, PageRowCY(0)); CHECK(g_kind == SK_PAGE && g_pageTitle == "Game");
    fakeEscape = true; Frame(false, 0, 0); fakeEscape = false; Idle(40);
    CHECK(depth == 2 && g_kind == SK_OPTIONS);                              // Android back popped the page
    fakeEscape = true; Frame(false, 0, 0); fakeEscape = false; Idle(40);
    CHECK(depth == 1 && g_kind == SK_ROOT);
    fakeEscape = true; Frame(false, 0, 0); fakeEscape = false; Idle(40);
    CHECK(depth == 1 && g_kind == SK_ROOT);                                 // on the root view the key is left to the stock menu (resume)

    // =================================================================================== S4 mods list and settings
    printf("S4  Mods -> MODS SETTINGS -> back to the Mods list\n");
    Tap(L.cx, rootY(6)); CHECK(g_view == VIEW_MODS);
    CHECK(!g_modRows.empty() && g_modRows[0].kind == MR_SETTINGS);
    CHECK(g_modRows.size() >= 3 && g_modRows[1].kind == MR_TAB);
    Shot("4_mods");
    int files = 0; for(auto& r : g_modRows) if(r.kind == MR_FILE) ++files;
    CHECK(files == 4);
    ComputeLayout();
    int savedSel = 0; for(size_t i = 0; i < g_modRows.size(); ++i) if(g_modRows[i].label == "RADARROTATION") savedSel = (int)i;
    Tap(L.cx, modsY(savedSel));
    Idle(40);
    CHECK(g_nav == NAV_NONE && g_kind == SK_PAGE);
    CHECK(g_pageTitle == "Mods Settings" && g_pageRows.size() == 6);
    CHECK(g_sel == 1);                                                      // "RADAR ROTATION" row matches the RadarRotation mod
    Shot("5_modsettings");
    Tap(L.pBackL + 30, L.pBackT + 20); Idle(30);
    CHECK(depth == 1 && g_kind == SK_ROOT);
    CHECK(g_view == VIEW_MODS);                                             // back in the Mods list...
    CHECK(g_sel == savedSel);                                               // ...on the same row

    // =================================================================================== S5 per-mod section
    printf("S5  tapping a mod that has a section jumps to it\n");
    ComputeLayout();
    int vo = -1; for(size_t i = 0; i < g_modRows.size(); ++i) if(g_modRows[i].label == "VISUALOBJECT") vo = (int)i;
    CHECK(vo >= 0);
    Tap(L.cx, modsY(vo)); Idle(40);
    CHECK(g_kind == SK_PAGE);
    CHECK(g_sel == 3);                                                      // "VISUAL OBJECT V1.0.3"
    CHECK(g_toastFrames == 0 || g_toast.find("VISUALOBJECT") == std::string::npos);
    Tap(L.pBackL + 30, L.pBackT + 20); Idle(30);
    CHECK(depth == 1 && g_kind == SK_ROOT && g_view == VIEW_MODS);
    // a mod without any settings: everything is shown and a message says so
    ComputeLayout();
    int ze = -1; for(size_t i = 0; i < g_modRows.size(); ++i) if(g_modRows[i].label == "ZETA") ze = (int)i;
    CHECK(ze >= 0);
    Tap(L.cx, modsY(ze)); Idle(40);
    CHECK(g_kind == SK_PAGE && g_pageRows.size() == 6 && g_sel == 0);
    CHECK(g_toastFrames > 0 && g_toast.find("ZETA") != std::string::npos);
    Tap(L.pBackL + 30, L.pBackT + 20); Idle(30);
    CHECK(depth == 1 && g_kind == SK_ROOT && g_view == VIEW_MODS);
    // BACK row of the Mods list returns to the pause menu
    ComputeLayout(); Tap(L.cx, L.backY); CHECK(g_view == VIEW_MAIN);

    // =================================================================================== S6 robustness
    printf("S6  robustness\n");
    g_pcOptions = false; Tap(L.cx, rootY(5)); Idle(12);                      // with PCOptions=0 the stock Options screen is left alone
    CHECK(g_kind == SK_NONE); g_pcOptions = true; depth = 1; pending = NULL; Sync(); Idle(3); CHECK(g_kind == SK_ROOT);
    *(int*)(mmBuf + OFF_MM_SCREENCOUNT) = 7; Idle(2); CHECK(g_kind == SK_NONE); Sync();   // unknown depth: stock UI
    *(void**)(pDisp) = (void*)0xdead; depth = 3; stackArr[1] = optScr; stackArr[2] = pDisp; pending = NULL; Sync(); Idle(2);
    CHECK(g_kind == SK_NONE);                                               // unknown screen class: stock UI, no crash
    depth = 0; Sync(); Idle(2); CHECK(g_kind == SK_NONE);

    // =================================================================================== R1..R6 regressions from the device
    printf("R   regressions: back / transitions / no game\n");
    *(void**)pDisp = vtPage;                                                 // S6 corrupted this page on purpose
    // reset to a clean pause menu
    pending = NULL; depth = 0; menuClosed = false; Sync(); PushNow(rootScr); Idle(40); g_view = VIEW_MAIN;
    // R1: Mods -> MODS SETTINGS -> BACK must land on the Mods list, never on a closed menu (black screen)
    ComputeLayout(); Tap(L.cx, rootY(6)); CHECK(g_view == VIEW_MODS);
    Tap(L.cx, modsY(0)); Idle(60); CHECK(g_kind == SK_PAGE && g_nav == NAV_NONE);
    Tap(L.pBackL + 30, L.pBackT + 20); Idle(80);
    CHECK(!menuClosed); CHECK(depth == 1); CHECK(g_kind == SK_ROOT && g_view == VIEW_MODS);
    ComputeLayout(); Tap(L.cx, L.backY); CHECK(g_view == VIEW_MAIN);
    // R2: Options -> Display -> BACK, tapped twice in a row while the first transition is still running: must stop at Options
    Tap(L.cx, rootY(5)); ComputeLayout(); Tap(L.pOptCx, PageRowCY(1)); CHECK(g_kind == SK_PAGE && depth == 3);
    QuickTap(L.pBackL + 30, L.pBackT + 20);                                    // first BACK
    Frame(false, 0, 0); QuickTap(L.pBackL + 30, L.pBackT + 20);               // second BACK 2 frames later (transition still running)
    Idle(80);
    CHECK(!menuClosed); CHECK(depth == 2 && g_kind == SK_OPTIONS);
    // R3: hammering BACK never closes the menu or pops past the root (start from a page again)
    ComputeLayout(); Tap(L.pOptCx, PageRowCY(2)); CHECK(g_kind == SK_PAGE && depth == 3);
    for(int i = 0; i < 12; ++i) { QuickTap(L.pBackL + 30, L.pBackT + 20); Frame(false, 0, 0); }
    Idle(80); CHECK(!menuClosed); CHECK(depth >= 1);
    // R4: the Android back key during a transition is ignored (no double pop)
    pending = NULL; depth = 0; menuClosed = false; Sync(); PushNow(rootScr); Idle(40);
    Tap(L.cx, rootY(5)); ComputeLayout(); Tap(L.pOptCx, PageRowCY(0)); CHECK(g_kind == SK_PAGE && depth == 3);
    fakeEscape = true; Frame(false, 0, 0); Frame(false, 0, 0); Frame(false, 0, 0); fakeEscape = false; Idle(80);
    CHECK(!menuClosed); CHECK(depth >= 2);
    // R5: tapping an Options tile twice quickly pushes only one screen
    pending = NULL; depth = 0; menuClosed = false; Sync(); PushNow(rootScr); Idle(40);
    Tap(L.cx, rootY(5)); ComputeLayout(); CHECK(g_kind == SK_OPTIONS);
    QuickTap(L.pOptCx, PageRowCY(1)); Frame(false, 0, 0); QuickTap(L.pOptCx, PageRowCY(1)); Idle(80);
    CHECK(depth == 3);
    // R6: Stats / Brief / Map need a running game (null player -> crash in CStats::ConstructStatLine otherwise)
    pending = NULL; depth = 0; menuClosed = false; Sync(); PushNow(rootScr); Idle(40); g_view = VIEW_MAIN; calls.clear();
    playing = false; ComputeLayout();
    Tap(L.cx, rootY(3)); Tap(L.cx, rootY(4));
    CHECK(std::find(calls.begin(), calls.end(), "stats") == calls.end() && std::find(calls.begin(), calls.end(), "brief") == calls.end());
    CHECK(g_toastFrames > 0);
    Tap(L.cx, rootY(2)); CHECK(g_view == VIEW_MAIN);                           // MAP is also refused
    playing = true; Tap(L.cx, rootY(3)); CHECK(std::count(calls.begin(), calls.end(), "stats") == 1);
    Tap(L.cx, rootY(4)); CHECK(std::count(calls.begin(), calls.end(), "brief") == 1);

    // =================================================================================== R7..R9: second round of device reports
    printf("R2  stock BackPressed() with a finger on the Back corner / mod tabs that read the chosen tile\n");
    auto ResetToRoot = [&]() { pending = NULL; depth = 0; menuClosed = false; stockBackCalls = 0; Sync(); PushNow(rootScr); Idle(40); g_view = VIEW_MAIN; ComputeLayout(); };
    for(int variant = 0; variant < 2; ++variant)
    {
#ifdef OLD_SOURCE
        if(variant == 1) break;                                              // the old code has no BackPressed hook
#endif
        useBackHook = (variant == 1);
        printf("   (stock BackPressed %s)\n", useBackHook ? "hooked" : "not hooked: coordinates alone must keep it quiet");
        // R7: the finger rests on our BACK button (same corner as the stock one) for a while, then lifts
        ResetToRoot(); Tap(L.cx, rootY(5)); ComputeLayout(); Tap(L.pOptCx, PageRowCY(1)); CHECK(g_kind == SK_PAGE && depth == 3);
        for(int i = 0; i < 25; ++i) Frame(true, OverX(), OverY());
        Frame(false, OverX(), OverY()); Idle(120);
        CHECK(!menuClosed); CHECK(depth == 2 && g_kind == SK_OPTIONS);        // exactly one level up
        Idle(300); CHECK(depth == 2 && !menuClosed);                         // and the stale finger position never pops anything later
        // R8: a quick tap on BACK, finger lifting inside the Back corner: still exactly one level
        ResetToRoot(); Tap(L.cx, rootY(5)); ComputeLayout(); Tap(L.pOptCx, PageRowCY(0)); CHECK(g_kind == SK_PAGE && depth == 3);
        Tap(OverX(), OverY()); Idle(200);
        CHECK(!menuClosed); CHECK(depth == 2 && g_kind == SK_OPTIONS);
        // ... and BACK on the Options screen goes to the root menu (one level), not further
        Tap(OverX(), OverY()); Idle(200);
        CHECK(!menuClosed); CHECK(depth == 1 && g_kind == SK_ROOT);
    }
    useBackHook = false;
    // R9: tiles of mods (SAUtils' OnTabButtonClicked / OnCustomModSettingsOpened) find their tab through the screen's chosen tile
    ResetToRoot(); Tap(L.cx, rootY(5)); ComputeLayout(); CHECK(g_kind == SK_OPTIONS && g_optRows.size() == 6);
    tabSeenOpt = -999; Tap(L.pOptCx, PageRowCY(5)); CHECK(tabSeenOpt == 52);          // tile 5 ("B")
    tabSeenOpt = -999; Tap(L.pOptCx, PageRowCY(4)); CHECK(tabSeenOpt == 41);          // tile 4 ("A")
    tabSeenOpt = -999; Tap(L.pOptCx, PageRowCY(0)); Idle(60); CHECK(g_kind == SK_PAGE); // a stock tile still works afterwards
    Tap(L.pBackL + 30, L.pBackT + 20); Idle(60);
    Tap(L.pBackL + 30, L.pBackT + 20); Idle(60); CHECK(depth == 1 && g_kind == SK_ROOT);
    // the same for a settings tab that lives on the root screen (Mods list)
    ComputeLayout(); Tap(L.cx, rootY(6)); CHECK(g_view == VIEW_MODS);
    int tabRow = -1; for(size_t i = 0; i < g_modRows.size(); ++i) if(g_modRows[i].kind == MR_TAB) tabRow = (int)i;
    CHECK(tabRow >= 0); tabSeenRoot = -999; ComputeLayout(); Tap(L.cx, modsY(tabRow)); CHECK(tabSeenRoot == 6);

    // =================================================================================== R10: Start Game flow (v1.1.3)
    printf("R3  Start Game -> New / Load / Delete -> slot list -> confirm dialog, all PC style\n");
    for(int i = 0; i < 16; ++i) vtStart[i] = vtLoadScr[i] = vtDelScr[i] = vtYesNo[i] = vtSaveScr[i] = vtSlot[i] = (void*)0x1000;
    vtStart[9] = vtLoadScr[9] = vtDelScr[9] = vtYesNo[9] = vtSaveScr[9] = (void*)fOnBack;
    vtStart[11] = (void*)fFlowYes; vtLoadScr[11] = vtDelScr[11] = vtYesNo[11] = vtSaveScr[11] = (void*)fFlowNo;
    vtSlot[5] = (void*)fSlotTitle; vtSlot[6] = (void*)fSlotInput;
    g_vtStartGame = (uintptr_t)vtStart; g_selTitle[0] = "";
    g_selVts[1] = (uintptr_t)vtLoadScr; g_selStyle[1] = PS_SLOTS; g_selTitle[1] = "Load Game";
    g_selVts[2] = (uintptr_t)vtDelScr;  g_selStyle[2] = PS_SLOTS; g_selTitle[2] = "Delete Game";
    g_selVts[3] = (uintptr_t)vtYesNo;   g_selStyle[3] = PS_DIALOG; g_selTitle[3] = "";
    g_selVts[4] = (uintptr_t)vtSaveScr; g_selStyle[4] = PS_SLOTS; g_selTitle[4] = "Save Game"; g_selVtCount = 5;
    g_slotSelVts[0] = (uintptr_t)vtSlot; g_slotSelCount = 1; g_pLastDevice = &fakeLastDevice;
    *(void**)startScr = vtStart;
    startItems[0] = { NULL, "FES_NEW", cNewGame }; startItems[1] = { NULL, "FES_LOA", cLoadPush }; startItems[2] = { NULL, "FES_DEL", cDelPush };
    *(int*)(startScr + OFF_SCR_ITEMCOUNT) = 3; *(StockItem**)(startScr + OFF_SCR_ITEMS) = startItems;
    SetupSlots(loadScr, vtLoadScr, loadItems); SetupSlots(delScr, vtDelScr, delItems); SetupSlots(saveScr, vtSaveScr, saveItems);
    *(void**)yesScr = vtYesNo; *(const char**)(yesScr + OFF_MSG) = "FESZ_QL";
    yesItems[0] = MkAction("FEM_YES", yesFunc, 0); yesItems[1] = MkAction("FEM_NO", noFunc, 0);
    *(int*)(yesScr + SEL_ITEMCOUNT) = 2; *(void***)(yesScr + SEL_ITEMS) = yesItems;
    fnOnStartGame = cStartPush; rootItems[1].cb = cStartPush; g_cacheTop = NULL;       // the stock START NEW GAME tile opens the Game screen
    g_cfgModsInMain = false;
    ResetToRoot(); calls.clear();
    CHECK(g_entries.size() == 7);
    Tap(L.cx, rootY(1));                                                       // START NEW GAME
    CHECK(std::count(calls.begin(), calls.end(), "startscreen") == 1);
    CHECK(g_kind == SK_START && g_style == PS_TILES_LEFT && depth == 2);        // used to fall back to the stock screen
    CHECK(g_optRows.size() == 3 && g_optRows[0].label == "NEW GAME" && g_optRows[1].label == "LOAD GAME" && g_optRows[2].label == "DELETE GAME");
    Shot("6_startgame");
    ComputeLayout();
    CHECK(PageRowCY(0) > L.sTop && PageRowCY(2) < 0.88f * L.H);
    Tap(L.sLabelX + 60, PageRowCY(1));                                          // LOAD GAME
    CHECK(g_kind == SK_PAGE && g_style == PS_SLOTS && depth == 3);
    CHECK(std::string(g_slotTitle) == "Load Game");
    CHECK(g_pageRows.size() == 9 && g_pageRows[1].kind == 3);
    CHECK(g_pageRows.size() == 9 && g_pageRows[0].label == "AUTOSAVE: EMPTY SLOT" && g_pageRows[1].label == "BMX" && g_pageRows[8].label == "CLOUD NOT AVAILABLE");
    Shot("7_loadgame");
    // a slot row: the stock row's HandleInput runs (guard word is cleared for the call and restored afterwards), the confirm dialog comes up
    *(int*)(mmBuf + MM_PSTATE) = 1; slotInputs = 0;
    ComputeLayout(); Tap(L.sLabelX + 60, PageRowCY(1));
    CHECK(slotInputs == 1 && lastSlotHit == 1);
    CHECK(*(int*)(mmBuf + MM_PSTATE) == 1);
    *(int*)(mmBuf + MM_PSTATE) = 0;
    CHECK(g_kind == SK_PAGE && g_style == PS_DIALOG && depth == 4);
    CHECK(g_dlgMsg == "ARE YOU SURE YOU WANT TO LOAD THIS GAME?");
    CHECK(g_pageRows.size() == 2 && g_pageRows[0].label == "YES" && g_pageRows[1].label == "NO");
    Shot("8_dialog");
    // the same tap again while the dialog is up must not hit the slot row beneath it
    int before = slotInputs; ComputeLayout();
    Tap(L.cx, PageRowCY(1));                                                    // NO
    CHECK(noCalls == 1 && yesCalls == 0 && slotInputs == before);
    CHECK(g_kind == SK_PAGE && g_style == PS_SLOTS && depth == 3);
    // YES goes through the dialog's own callback too
    ComputeLayout(); Tap(L.sLabelX + 60, PageRowCY(2)); CHECK(g_style == PS_DIALOG && depth == 4);
    ComputeLayout(); Tap(L.cx, PageRowCY(0)); CHECK(yesCalls == 1 && depth == 3 && g_style == PS_SLOTS);
    // BACK from the slot list -> Game tiles -> pause menu: one level per tap, never past the root
    Tap(L.pBackL + 30, L.pBackT + 20); Idle(60); CHECK(depth == 2 && g_kind == SK_START && !menuClosed);
    Tap(L.pBackL + 30, L.pBackT + 20); Idle(60); CHECK(depth == 1 && g_kind == SK_ROOT && !menuClosed);
    // Android back key does the same
    Tap(L.cx, rootY(1)); ComputeLayout(); Tap(L.sLabelX + 60, PageRowCY(2)); CHECK(g_kind == SK_PAGE && g_style == PS_SLOTS && std::string(g_slotTitle) == "Delete Game" && depth == 3);
    fakeEscape = true; Frame(false, 0, 0); fakeEscape = false; Idle(60); CHECK(depth == 2 && g_kind == SK_START);
    fakeEscape = true; Frame(false, 0, 0); fakeEscape = false; Idle(60); CHECK(depth == 1 && g_kind == SK_ROOT);
    // NEW GAME runs the stock tile callback (which unloads the whole menu) and nothing touches the freed screens afterwards
    Tap(L.cx, rootY(1)); ComputeLayout(); newGameCalls = 0; Tap(L.sLabelX + 60, PageRowCY(0));
    CHECK(newGameCalls == 1); Idle(10); CHECK(g_kind == SK_NONE && depth == 0);
    // text colours: the pause menu and the map overlay are white/steel-blue now, never gold
    opsBuf.clear(); ResetToRoot(); Idle(2); opsBuf.clear(); Frame(false, 0, 0);
    CHECK(opsBuf.find(" 171 131 12 255") == std::string::npos);
    ComputeLayout(); Tap(L.cx, rootY(2)); CHECK(g_view == VIEW_MAP); opsBuf.clear(); Frame(false, 0, 0);
    CHECK(opsBuf.find(" 171 131 12 255") == std::string::npos);
    StockBackTap(); CHECK(g_view == VIEW_MAIN);
    // unknown rows in a slot list / dialog: the stock screen stays (nothing to show = nothing hidden)
    ResetToRoot(); Tap(L.cx, rootY(1)); ComputeLayout(); Tap(L.sLabelX + 60, PageRowCY(1)); CHECK(g_kind == SK_PAGE && depth == 3);
    *(void**)(loadItems[0]) = (void*)vtOther; for(int i = 1; i < 9; ++i) *(void**)(loadItems[i]) = (void*)vtOther; Idle(40);
    CHECK(g_kind == SK_NONE);
    for(int i = 0; i < 9; ++i) *(void**)(loadItems[i]) = (void*)vtSlot; g_pageRowsTop = NULL; Idle(3); CHECK(g_kind == SK_PAGE);   // (a fresh screen: rows are rebuilt)
    // a Save screen that is the only screen (save point in the game) is not touched
    pending = NULL; depth = 0; Sync(); PushNow(saveScr); Idle(3); CHECK(g_kind == SK_NONE);
    pending = NULL; depth = 0; menuClosed = false; Sync();

    // =================================================================================== R11: stock map mode (v1.1.4)
    printf("R4  stock map mode: Stats / Brief must never be left blank and dead\n");
    {
        alignas(8) static uint8_t statsScr[256], briefScr[256]; static void* vtStats[16];
        for(int i = 0; i < 16; ++i) vtStats[i] = (void*)0x1000;
        vtStats[9] = (void*)fOnBack; vtStats[11] = (void*)fFlowNo;                     // StatsScreen / BriefScreen: SelectScreens, IsFlow() == false
        *(void**)statsScr = vtStats; *(void**)briefScr = vtStats;
        auto Alive = [&](int frames) { int u = topUpdates, r = topRenders; Idle(frames); return topUpdates > u && topRenders > r; };
        auto Reset = [&]() { pending = NULL; depth = 0; menuClosed = false; Sync(); PushNow(rootScr); g_cfgEnabled = true; g_pcOptions = true; playing = true; g_view = VIEW_MAIN; Idle(5); ComputeLayout(); };
        auto cStatsPush = [](){ calls.push_back("stats"); Push(statsScr); };
        auto cBriefPush = [](){ calls.push_back("brief"); Push(briefScr); };
        fnOnStats = cStatsPush; fnOnBriefs = cBriefPush; rootItems[2].cb = cStatsPush; rootItems[3].cb = cBriefPush; g_cacheTop = NULL;
        // colours sampled from the real PC "Main Menu" screenshot: title + selected item (172,203,241), unselected items (74,90,107)
        Reset(); opsBuf.clear(); Frame(false, 0, 0);
        CHECK(opsBuf.find(" 172 203 241 255\n") != std::string::npos && opsBuf.find(" 74 90 107 255\n") != std::string::npos);
        CHECK(opsBuf.find(" 182 214 238 255\n") == std::string::npos && opsBuf.find(" 96 105 115 255\n") == std::string::npos);   // the old blues are gone
        Shot("0_pause_colors");
        // a) the game opens the pause menu in map mode (InitForPause with the radar widget visible): the PC main view switches it off at once
        Reset(); mmBuf[OFF_MM_MAPMODE] = 1; Idle(2);
        CHECK(mmBuf[OFF_MM_MAPMODE] == 0);
        // b) ... so Stats opened right away is drawn and updated (this is the reported bug: blank, no way back)
        mmBuf[OFF_MM_MAPMODE] = 1; calls.clear(); ComputeLayout(); Tap(L.cx, rootY(3));
        CHECK(std::count(calls.begin(), calls.end(), "stats") == 1 && depth == 2 && g_kind == SK_NONE);
        CHECK(mmBuf[OFF_MM_MAPMODE] == 0); CHECK(Alive(4));
        // c) even if map mode comes back while a non-flow screen is on top, it is cleared again (the stock code can never leave it there)
        mmBuf[OFF_MM_MAPMODE] = 1; CHECK(Alive(3)); CHECK(mmBuf[OFF_MM_MAPMODE] == 0);
        // d) Brief behaves the same
        Reset(); mmBuf[OFF_MM_MAPMODE] = 1; calls.clear(); ComputeLayout(); Tap(L.cx, rootY(4));
        CHECK(std::count(calls.begin(), calls.end(), "brief") == 1 && depth == 2 && Alive(4) && mmBuf[OFF_MM_MAPMODE] == 0);
        // e) MAP: the stock map mode is switched on (map visible + scrollable at once) and left alone while our MAP view is up ...
        Reset(); Tap(L.cx, rootY(2)); CHECK(g_view == VIEW_MAP); CHECK(mmBuf[OFF_MM_MAPMODE] == 1);
        Idle(30); CHECK(g_view == VIEW_MAP && mmBuf[OFF_MM_MAPMODE] == 1);
        //    ... and BACK switches it off again, so the buttons that follow (Stats) work
        ComputeLayout(); StockBackTap(); Idle(2); CHECK(g_view == VIEW_MAIN && mmBuf[OFF_MM_MAPMODE] == 0);
        calls.clear(); ComputeLayout(); Tap(L.cx, rootY(3)); CHECK(std::count(calls.begin(), calls.end(), "stats") == 1 && Alive(4));
        // f) the Android back key leaves the MAP view the same way
        Reset(); Tap(L.cx, rootY(2)); CHECK(g_view == VIEW_MAP && mmBuf[OFF_MM_MAPMODE] == 1);
        fakeEscape = true; Frame(false, 0, 0); fakeEscape = false; Idle(3); CHECK(g_view == VIEW_MAIN && mmBuf[OFF_MM_MAPMODE] == 0);
        // g) Options (a FlowScreen) and the Mods list never run in map mode either
        Reset(); mmBuf[OFF_MM_MAPMODE] = 1; ComputeLayout(); Tap(L.cx, rootY(5)); CHECK(g_kind == SK_OPTIONS && mmBuf[OFF_MM_MAPMODE] == 0);
        // h) when the PC menu is not active (stock UI) a flow screen keeps whatever map mode the game chose
        Reset(); g_cfgEnabled = false; mmBuf[OFF_MM_MAPMODE] = 1; Idle(3); CHECK(g_kind == SK_NONE && mmBuf[OFF_MM_MAPMODE] == 1);
        g_cfgEnabled = true; mmBuf[OFF_MM_MAPMODE] = 0;
        // i) an unreadable / unknown top screen never crashes the check
        Reset(); *(void**)(pDisp) = (void*)0xdead; depth = 2; stackArr[1] = pDisp; pending = NULL; Sync(); mmBuf[OFF_MM_MAPMODE] = 1; Idle(3);
        CHECK(g_kind == SK_NONE && mmBuf[OFF_MM_MAPMODE] == 1);          // cannot tell: left alone
        mmBuf[OFF_MM_MAPMODE] = 0; Reset();
    }

    // =================================================================================== R12: v1.1.5
    printf("R5  button position editor / no slide select / gang territory note\n");
    {
        *(void**)pDisp = vtPage;                                                      // (R4 i corrupted this page on purpose)
        alignas(8) static uint8_t pCtl[256]; static void* ctlItems[4];
        ctlItems[0] = MkAction("Change button position", fToggleEditor, 0); ctlItems[1] = MkSetting("Show FPS", 3); SetupPage(pCtl, ctlItems, 2);
        auto Reset = [&]() { pending = NULL; depth = 0; menuClosed = false; fakeAdj = NULL; editorCloseTap = false; Sync(); PushNow(rootScr); g_cfgEnabled = true; g_pcOptions = true; playing = true; g_view = VIEW_MAIN; Idle(40); ComputeLayout(); };
        auto OpenControls = [&]() { Reset(); Tap(L.cx, rootY(5)); Idle(5); Push(pCtl); Idle(40); ComputeLayout(); };

        // ---- 2. CHANGE BUTTON POSITION opens the stock editor and the PC page steps aside
        OpenControls();
        CHECK(g_kind == SK_PAGE && g_pageRows.size() == 2 && g_pageRows[0].kind == 0);
        Tap(L.pLabelX + 60, PageRowCY(0));
        CHECK(fakeAdj != NULL);                                                       // the stock Toggle ran
        int eu = editorUpdates, er = editorRenders; drawSprites = drawQuads = 0; opsBuf.clear();
        Idle(3);
        CHECK(g_kind == SK_NONE && g_editorUp);                                       // the editor owns the screen...
        CHECK(drawSprites == 0 && drawQuads == 0);                                    // ...so nothing of the PC page is painted over it
        CHECK(editorUpdates == eu + 3 && editorRenders == er + 3);                    // the stock update / render ran for it every frame
        g_sel = 1;                                                                    // (what the page had selected stays selected behind the editor)
        Frame(true, 100, 100); CHECK(seenInStock == 2);                               // the editor sees the REAL finger (the PC page would blind it)
        CHECK(fakeAdj != NULL); Frame(false, 0, 0);
        editorCloseTap = true; Frame(true, 100, 100); Frame(false, 0, 0); editorCloseTap = false;   // press on the editor's DONE
        CHECK(fakeAdj == NULL);
        Idle(3); CHECK(g_kind == SK_PAGE && g_activeTop == (void*)pCtl);              // the PC page is back...
        CHECK(g_sel == 1);                                                            // ...exactly as it was left
        drawSprites = 0; Frame(false, 0, 0); CHECK(drawSprites > 0);
        // the back key while the editor is up reaches the stock code (CAdjustableHUD::Update handles it itself), the PC menu does not eat it
        Tap(L.pLabelX + 60, PageRowCY(0)); CHECK(fakeAdj != NULL);
        fakeEscape = true; Frame(false, 0, 0); fakeEscape = false; CHECK(g_backRequested == false);
        fakeAdj = NULL; Idle(3);
        // the same page without the symbol: nothing breaks (the editor just cannot be told apart)
        g_ppAdjHUD = NULL; Idle(2); CHECK(g_kind == SK_PAGE); g_ppAdjHUD = &fakeAdj;

        // ---- 3. no slide select: a finger that slides over the root menu rows never moves the highlight or activates a neighbour
        Reset(); calls.clear(); g_cfgModsInMain = false; Idle(3); ComputeLayout();
        CHECK(g_entries.size() == 7 && g_entries[3].label == "STATS" && g_entries[5].label == "OPTIONS");
        g_sel = 0; int osc0 = onSettingsCalls;
        Frame(true, L.cx, rootY(3)); CHECK(g_sel == 3);                               // touching a text button highlights it
        Frame(true, L.cx, rootY(4)); Frame(true, L.cx, rootY(5)); CHECK(g_sel == 3);  // sliding down: the highlight stays where the finger went down
        Frame(true, L.cx, rootY(5)); Frame(false, L.cx, rootY(5)); Idle(12);
        CHECK(calls.empty() && onSettingsCalls == osc0 && g_kind == SK_ROOT && g_sel == 3);   // nothing opened
        {   // slide away and come back to the same row: still not a tap
            int osc = onSettingsCalls;
            Frame(true, L.cx, rootY(3)); Frame(true, L.cx, rootY(5)); Frame(true, L.cx, rootY(3)); Frame(false, L.cx, rootY(3)); Idle(12);
            CHECK(calls.empty() && onSettingsCalls == osc);
        }
        // a real tap with a little finger wobble (well inside the slop) works
        Frame(true, L.cx, rootY(3)); Frame(true, L.cx + 6, rootY(3) + 5); Frame(true, L.cx - 4, rootY(3) - 4); Frame(false, L.cx - 4, rootY(3) - 4); Idle(12);
        CHECK(std::count(calls.begin(), calls.end(), "stats") == 1);
        // ... and so does a plain tap on each other button (OPTIONS is a tile that pushes the Options screen)
        Reset(); calls.clear(); int osc2 = onSettingsCalls; Tap(L.cx, rootY(5)); CHECK(onSettingsCalls == osc2 + 1);
        // a tap that goes up on another row than it went down on does nothing
        Reset(); calls.clear(); Frame(true, L.cx, rootY(3)); Frame(false, L.cx, rootY(4)); Idle(12); CHECK(calls.empty());
        // pages: a finger dragged sideways is not a tap either, a still one is
        OpenControls();
        Frame(true, L.pLabelX + 60, PageRowCY(0)); Frame(true, L.pLabelX + 60 + 0.25f * L.W, PageRowCY(0)); Frame(false, L.pLabelX + 60 + 0.25f * L.W, PageRowCY(0)); Idle(12);
        CHECK(fakeAdj == NULL && g_kind == SK_PAGE);
        Frame(true, L.pLabelX + 60, PageRowCY(0)); Frame(true, L.pLabelX + 66, PageRowCY(0) + 3); Frame(false, L.pLabelX + 66, PageRowCY(0) + 3); Idle(4);
        CHECK(fakeAdj != NULL); fakeAdj = NULL; Idle(3);
        // the Options tiles and the Mods list behave the same
        Reset(); Tap(L.cx, rootY(5)); ComputeLayout(); CHECK(g_kind == SK_OPTIONS);
        int dpth = depth;
        Frame(true, L.pOptCx, PageRowCY(1)); Frame(true, L.pOptCx + 0.3f * L.W, PageRowCY(1)); Frame(false, L.pOptCx + 0.3f * L.W, PageRowCY(1)); Idle(40);
        CHECK(depth == dpth && g_kind == SK_OPTIONS);                                 // sideways drag on a tile: no page opened
        Tap(L.pOptCx, PageRowCY(1)); Idle(40); CHECK(depth == dpth + 1 && g_kind == SK_PAGE);

        // ---- 1. (v1.1.6) the "no gang territory" note is gone, the map view has no PC BACK, the stock Back (bottom right) leaves the map
        Reset(); fakeGang = 0; fakeZones = 100; g_toastFrames = 0; calls.clear();
        Tap(L.cx, rootY(2)); CHECK(g_view == VIEW_MAP && mmBuf[OFF_MM_MAPMODE] == 1);
        CHECK(g_toastFrames == 0);                                                    // no note about gang wars any more
        drawSprites = drawQuads = 0; opsBuf.clear(); Frame(false, 0, 0);
        CHECK(drawSprites == 0 && drawQuads == 0);                                    // nothing of ours over the stock map: no PC BACK button, no note
        Frame(true, 300, 200); CHECK(seenInStock == 2); Frame(true, 340, 230); CHECK(seenInStock == 3); Frame(false, 340, 230);   // the stock map gets the real finger (scrolling)
        Idle(3); CHECK(g_view == VIEW_MAP && mmBuf[OFF_MM_MAPMODE] == 1);
        { int bc = backCalls; StockBackTap(); CHECK(g_view == VIEW_MAIN && mmBuf[OFF_MM_MAPMODE] == 0 && backCalls == bc); }   // the stock Back switched the map off, the PC main view is back
        drawSprites = 0; Frame(false, 0, 0); CHECK(drawSprites > 0);                  // ...and drawn again
        Reset(); fakeGang = 1; g_toastFrames = 0; Tap(L.cx, rootY(2)); CHECK(g_view == VIEW_MAP && g_toastFrames == 0);
        Reset(); fakeGang = 0; g_pGangWars = NULL; g_toastFrames = 0; Tap(L.cx, rootY(2)); CHECK(g_view == VIEW_MAP && g_toastFrames == 0);
        g_pGangWars = &fakeGang; fakeGang = 1; Reset();
    }

    // =================================================================================== R6: v1.1.6 languages
    printf("R6  PC menu texts follow the game language, Indonesian is added\n");
    {
        auto Reset = [&]() { pending = NULL; depth = 0; menuClosed = false; fakeAdj = NULL; Sync(); PushNow(rootScr); g_cfgEnabled = true; g_pcOptions = true; playing = true; g_view = VIEW_MAIN; g_cfgModsInMain = false; Idle(40); ComputeLayout(); };
        auto SetLang = [&](int l) { gameLang = l; fakeFront[OFF_FE_LANGUAGE] = (uint8_t)l; };
        auto Labels = [&]() { std::string r; for(auto& e : g_entries) r += (r.empty() ? "" : "|") + e.label; return r; };
        auto ReadFile = [&](const char* path) { std::string t; FILE* f = fopen(path, "rb"); if(f) { char b[512]; size_t n; while((n = fread(b, 1, sizeof b, f)) > 0) t.append(b, n); fclose(f); } return t; };
        *(void**)pDisp = vtPage; *(void**)pGame = vtPage; *(void**)pAudio = vtPage; g_cfgLang = 0; SetLang(0); *(uint8_t*)(rootScr + OFF_SCR_ISPAUSE) = 1;

        // GXT characters: the game's code page (read from CText::GetUpperCase) folds to plain ASCII
        { bool ok = true;
          unsigned short w1[] = { 'P', 'a', 'r', 'a', 'm', 0x9e, 't', 'r', 'e', 's', 0 };
          CHECK(GxtToAscii(w1, &ok) == "Parametres" && ok);
          unsigned short w2[] = { 0xAF, 'Q', 'u', 0x96, 0xAD, '~', 'w', '~', 0x80, 0xB0, '!', 0 };
          CHECK(GxtToAscii(w2, &ok) == "QussNA!" && ok);
          unsigned short w3[] = { 'A', 0x0416, 'B', 0 };
          CHECK(GxtToAscii(w3, &ok) == "AB" && !ok);                                           // (v1.1.8: what cannot be drawn is dropped, never printed as '?')
          unsigned short w4[] = { 0x97, 0x9e, 0xa5, 0xac, 0xae, 0x85, 0x95, 0 };                // a-grave e-acute o-grave u-umlaut n-tilde C-cedilla U-umlaut
          CHECK(GxtToAscii(w4, &ok) == "aeounCU" && ok); }

        // English game: the PC wording stays exactly as before
        Reset(); CHECK(UiLang() == LG_EN);
        CHECK(Labels() == "RESUME|START NEW GAME|MAP|STATS|BRIEF|OPTIONS|QUIT GAME");
        CHECK(TitleOf("MAIN MENU") == "Main Menu" && TitleOf("PAUSE MENU") == "Pause Menu" && TitleOf("MODS") == "Mods" && TitleOf("LOAD GAME") == "Load Game" && TitleOf("GAME") == "Game");
        CHECK(Tr("BACK") == "BACK" && OptionsTitle() == "Options");

        // French: the game's own text for the stock keys (accents folded), the table for "START NEW GAME" (no stock key of its own)
        SetLang(1); Idle(3); CHECK(UiLang() == LG_FR);
        CHECK(Labels() == "REPRENDRE|NOUVELLE PARTIE|CARTE|STATISTIQUES|BRIEFING|OPTIONS|QUITTER LE JEU"); Shot("14_fr_pause");
        CHECK(TitleOf("PAUSE MENU") == "Menu Pause" && TitleOf("MAIN MENU") == "Menu Principal" && Tr("BACK") == "RETOUR");
        *(uint8_t*)(rootScr + OFF_SCR_ISPAUSE) = 0; Idle(3); CHECK(Labels() == "REPRENDRE|COMMENCER LE JEU|OPTIONS|QUITTER LE JEU");   // title screen
        *(uint8_t*)(rootScr + OFF_SCR_ISPAUSE) = 1; Idle(3);
        ComputeLayout(); Tap(L.cx, rootY(5)); CHECK(g_kind == SK_OPTIONS);
        CHECK(g_optRows.size() == 6 && g_optRows[0].title == "Jeu" && g_optRows[1].title == "Affichage" && g_optRows[2].title == "Audio");
        ComputeLayout(); Tap(L.pOptCx, PageRowCY(1));
        CHECK(g_kind == SK_PAGE && g_pageTitle == "Affichage" && g_pageBg == 3);
        Idle(2); CHECK(g_pageRows.size() == 4 && g_pageRows[2].valText == "DESACTIVE");                  // toggle values come from the GXT too ("D<e-acute>sactiv<e-acute>")
        // the language changes while the Options screen is open: its tiles are rebuilt (the game may reload its text a few frames later)
        Tap(L.pBackL + 30, L.pBackT + 20); Idle(40); CHECK(g_kind == SK_OPTIONS);
        SetLang(0); Idle(40); CHECK(g_optRows.size() == 6 && g_optRows[0].title == "Game" && g_optRows[1].title == "Display");
        SetLang(1); Idle(40); CHECK(g_optRows[0].title == "Jeu");
        Reset(); CHECK(Labels() == "REPRENDRE|NOUVELLE PARTIE|CARTE|STATISTIQUES|BRIEFING|OPTIONS|QUITTER LE JEU");

        // German: no GXT entry for most keys -> the table; the ones the GXT has win
        SetLang(2); Idle(3); CHECK(Labels() == "FORTSETZEN|NEUES SPIEL STARTEN|KARTE|STATISTIKEN|BRIEFING|OPTIONEN|SPIEL BEENDEN");
        CHECK(Tr("BACK") == "ZURUCK" && TitleOf("PAUSE MENU") == "Pausenmenu");
        SetLang(3); CHECK(Tr("BACK") == "INDIETRO" && TitleOf("PAUSE MENU") == "Menu di Pausa");
        SetLang(4); CHECK(Tr("BACK") == "ATRAS" && TitleOf("PAUSE MENU") == "Menu de Pausa");

        // Russian / Japanese: the font has no glyphs -> English, never '?'
        SetLang(5); Idle(3); CHECK(Labels() == "RESUME|START NEW GAME|MAP|STATS|BRIEF|OPTIONS|QUIT GAME");
        ComputeLayout(); Tap(L.cx, rootY(5)); CHECK(g_kind == SK_OPTIONS && g_optRows.size() == 6 && g_optRows[0].title == "Game" && g_optRows[1].title == "Display");
        CHECK(Resolve("FEO_AUD") == "AUDIO" && Resolve("FEM_YES") == "YES");
        SetLang(6); CHECK(Tr("BACK") == "BACK");
        SetLang(0); Reset();
        fakeFront[OFF_FE_LANGUAGE] = 77; CHECK(GameLanguage() == -1 && UiLang() == LG_EN);                // an unreadable / odd value is treated as English
        fakeFront[OFF_FE_LANGUAGE] = 0;
        { uint8_t* keep = g_pFrontEnd; g_pFrontEnd = NULL; CHECK(GameLanguage() == -1 && UiLang() == LG_EN && Labels().size() > 0); g_pFrontEnd = keep; }

        // Indonesian (the game itself has no Indonesian text file: the PC menu translates what it draws)
        g_cfgLang = LG_ID; SetLang(0); Idle(3); CHECK(UiLang() == LG_ID);
        CHECK(Labels() == "LANJUTKAN|MULAI PERMAINAN BARU|PETA|STATISTIK|RINGKASAN|PENGATURAN|KELUAR PERMAINAN"); Shot("13_id_pause");
        CHECK(TitleOf("MAIN MENU") == "Menu Utama" && TitleOf("PAUSE MENU") == "Menu Jeda" && TitleOf("LOAD GAME") == "Muat Permainan" && TitleOf("MODS") == "Mod" && OptionsTitle() == "Pengaturan");
        CHECK(Tr("BACK") == "KEMBALI" && Tr("PLEASE WAIT") == "HARAP TUNGGU" && Tr("NO MODS FOUND") == "TIDAK ADA MOD DITEMUKAN" && Tr("NOTHING TO SHOW HERE") == "TIDAK ADA YANG DITAMPILKAN");
        CHECK(Tr("SOMETHING NOT IN THE TABLE") == "SOMETHING NOT IN THE TABLE");
        *(uint8_t*)(rootScr + OFF_SCR_ISPAUSE) = 0; Idle(3); CHECK(Labels() == "LANJUTKAN|MULAI PERMAINAN|PENGATURAN|KELUAR PERMAINAN"); *(uint8_t*)(rootScr + OFF_SCR_ISPAUSE) = 1; Idle(3);
        ComputeLayout(); Tap(L.cx, rootY(5)); CHECK(g_kind == SK_OPTIONS && g_optRows.size() == 6);
        CHECK(g_optRows[0].title == "Permainan" && g_optRows[1].title == "Tampilan" && g_optRows[2].title == "Audio" && g_optRows[3].title == "Pengaturan Mod");
        ComputeLayout(); Tap(L.pOptCx, PageRowCY(1));
        CHECK(g_kind == SK_PAGE && g_pageTitle == "Tampilan" && g_pageBg == 3);
        Idle(2);
        CHECK(g_pageRows.size() == 4 && g_pageRows[0].label == "EFEK VISUAL" && g_pageRows[1].label == "EFEK SUARA" && g_pageRows[3].label == "KEMBALIKAN KE BAWAAN");
        CHECK(g_pageRows[2].valText == "MATI"); Shot("15_id_display"); Tap(L.pLabelX + 60, PageRowCY(2)); Idle(2); CHECK(g_pageRows[2].valText == "NYALA");
        WriteSetting(3, 0);
        CHECK(Resolve("FESZ_QL") == "APAKAH ANDA YAKIN INGIN MEMUAT PERMAINAN INI?" && Resolve("FESZ_QL", false) == "Are you sure you want to load this game?");
        CHECK(Resolve("FEM_YES") == "YA" && Resolve("FEM_NO") == "TIDAK" && Resolve("Unknown Mod Text") == "Unknown Mod Text");
        CHECK(PhraseFor(LG_ID, "AUTOSAVE: EMPTY SLOT") == "SIMPAN OTOMATIS: SLOT KOSONG" && PhraseFor(LG_ID, "Brightness") == "KECERAHAN" && PhraseFor(LG_ID, "nothing known here").empty());
        // save slots and the Start Game tiles
        Reset(); Tap(L.cx, rootY(1)); ComputeLayout(); CHECK(g_kind == SK_START && g_optRows.size() == 3 && g_optRows[0].label == "PERMAINAN BARU" && g_optRows[1].label == "MUAT PERMAINAN" && g_optRows[2].label == "HAPUS PERMAINAN");
        Tap(L.sLabelX + 60, PageRowCY(1)); Idle(3);
        CHECK(g_kind == SK_PAGE && g_style == PS_SLOTS && g_pageRows.size() == 9);
        Idle(40); CHECK(g_pageRows[0].label == "SIMPAN OTOMATIS: SLOT KOSONG" && g_pageRows[1].label == "BMX" && g_pageRows[3].label == "SLOT KOSONG" && g_pageRows[7].label == "CLOUD TIDAK TERSEDIA");
        Shot("10_id_slots");
        // the Mods list texts
        Reset(); g_cfgModsInMain = true; Idle(3); CHECK(g_entries.size() == 8 && g_entries[6].label == "MOD"); ComputeLayout(); Tap(L.cx, rootY(6)); CHECK(g_view == VIEW_MODS && g_modRows[0].label == "PENGATURAN MOD");
        Shot("11_id_mods"); g_cfgModsInMain = false;

        // the Language page: "BAHASA INDONESIA" is one more row; picking a real language switches back to following the game
        alignas(8) static uint8_t pLang[256]; static void* vtLang[16]; static void* langItems[6];
        for(int i = 0; i < 16; ++i) vtLang[i] = (void*)0x1000;
        vtLang[9] = (void*)fOnBack; vtLang[11] = (void*)fFlowNo;
        static void (*pick)(void*, int) = [](void*, int d) { gameLang = d; fakeFront[OFF_FE_LANGUAGE] = (uint8_t)d; calls.push_back("lang" + std::to_string(d)); };
        langItems[0] = MkAction("FEL_ENG", pick, 0); langItems[1] = MkAction("FEL_FRE", pick, 1); langItems[2] = MkAction("FEL_RUS", pick, 5);
        *(void**)pLang = vtLang; *(int*)(pLang + SEL_ITEMCOUNT) = 3; *(void***)(pLang + SEL_ITEMS) = langItems;
        g_vtLanguage = (uintptr_t)vtLang; g_selVts[g_selVtCount] = (uintptr_t)vtLang; g_selStyle[g_selVtCount] = PS_SETTINGS; g_selTitle[g_selVtCount] = ""; ++g_selVtCount;
        std::string keepDir = g_dirPC; mkdir("/tmp/pcm_lang", 0777); g_dirPC = "/tmp/pcm_lang/";
        { FILE* f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs("Scale=1.0\r\nLanguage=Auto\r\nSounds=0\r\n", f); fclose(f); }
        g_cfgLang = 0; SetLang(0); Reset(); Tap(L.cx, rootY(5)); Idle(5); Push(pLang); Idle(40); ComputeLayout();
        CHECK(g_kind == SK_PAGE && g_activeVt == g_vtLanguage);
        CHECK(g_pageRows.size() == 4 && g_pageRows[0].label == "ENGLISH" && g_pageRows[2].label == "RUSSIAN" && g_pageRows[3].kind == 4 && g_pageRows[3].label == "BAHASA INDONESIA");   // (unrenderable Russian falls back to the key's English name)
        drawSprites = 0; Frame(false, 0, 0); CHECK(drawSprites > 0);
        Tap(L.pLabelX + 60, PageRowCY(3)); Idle(3);
        CHECK(g_cfgLang == LG_ID && UiLang() == LG_ID);
        { std::string ini = ReadFile("/tmp/pcm_lang/pcmenu.ini"); CHECK(ini.find("Language=Indonesian") != std::string::npos && ini.find("Language=Auto") == std::string::npos && ini.find("Scale=1.0") != std::string::npos && ini.find("Sounds=0") != std::string::npos); }
        g_pageRowsTop = NULL; Idle(3); CHECK(g_pageRows.size() == 4 && g_pageRows[3].label == "BAHASA INDONESIA");
        Shot("12_language_page");
        Tap(L.pLabelX + 60, PageRowCY(1)); Idle(3);                                                      // FRENCH: the stock action runs, the override is cleared and saved
        CHECK(g_cfgLang == 0 && gameLang == 1 && UiLang() == LG_FR && std::count(calls.begin(), calls.end(), "lang1") == 1);
        { std::string ini = ReadFile("/tmp/pcm_lang/pcmenu.ini"); CHECK(ini.find("Language=Auto") != std::string::npos && ini.find("Indonesian") == std::string::npos); }
        Tap(L.pLabelX + 60, PageRowCY(3)); Idle(3); CHECK(g_cfgLang == LG_ID);                                // Indonesian on top of a French game: the key table still names the tiles
        Reset(); CHECK(Labels() == "LANJUTKAN|MULAI PERMAINAN BARU|PETA|STATISTIK|RINGKASAN|PENGATURAN|KELUAR PERMAINAN");
        // ini round trips
        { remove("/tmp/pcm_lang/pcmenu.ini"); g_cfgLang = LG_ID; SaveLanguageSetting(); CHECK(ReadFile("/tmp/pcm_lang/pcmenu.ini") == "Language=Indonesian\n");
          g_cfgLang = 0; LoadConfig(); CHECK(g_cfgLang == LG_ID);
          FILE* f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs("Scale=1.2\nLanguage = Auto\nBackground=back8.png", f); fclose(f);
          g_cfgLang = LG_ID; SaveLanguageSetting(); CHECK(ReadFile("/tmp/pcm_lang/pcmenu.ini") == "Scale=1.2\nLanguage=Indonesian\nBackground=back8.png");
          f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs("Scale=1.2\nSounds=1", f); fclose(f);
          g_cfgLang = 0; SaveLanguageSetting(); CHECK(ReadFile("/tmp/pcm_lang/pcmenu.ini") == "Scale=1.2\nSounds=1\nLanguage=Auto\n");
          f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs("Language=ID\n", f); fclose(f); LoadConfig(); CHECK(g_cfgLang == LG_ID);
          f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs("Language=Bahasa Indonesia\n", f); fclose(f); g_cfgLang = 0; LoadConfig(); CHECK(g_cfgLang == LG_ID);
          f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs("Language=English\n", f); fclose(f); LoadConfig(); CHECK(g_cfgLang == 0); }
        g_dirPC = keepDir; g_cfgLang = 0; SetLang(0); --g_selVtCount; g_vtLanguage = 0; Reset();
        CHECK(Labels() == "RESUME|START NEW GAME|MAP|STATS|BRIEF|OPTIONS|QUIT GAME");
        CHECK(OwnerOf((uintptr_t)&OwnerOf) == "pcm_test" && OwnerOf(0) == "unknown");                    // hook owners are named in the log
    }

    // =================================================================================== R7: the PC menu is painted at the end of the stock pass
    printf("R7  other mods' overlays (ZyzyGFX settings icon) stay above the PC menu\n");
    {
        auto Reset = [&]() { pending = NULL; depth = 0; menuClosed = false; fakeAdj = NULL; Sync(); PushNow(rootScr); g_cfgEnabled = true; g_pcOptions = true; playing = true; g_view = VIEW_MAIN; g_cfgModsInMain = false; Idle(40); ComputeLayout(); };
        auto Lines = [](const std::string& t) { return (int)std::count(t.begin(), t.end(), '\n'); };
        static void (*overlayRender)(void*) = [](void* self) {            // a mod that hooked MobileMenu::Render before us: stock render, then its own icon
            fStockRender(self);
            V2 v[4] = { { 5000, 10 }, { 5100, 10 }, { 5100, 110 }, { 5000, 110 } }; Col c[4]; for(int k = 0; k < 4; ++k) c[k] = Col{ 9, 9, 9, 9 };
            fQuad(v, c); };
        Reset(); g_cfgLang = 0; gameLang = 0; fakeFront[OFF_FE_LANGUAGE] = 0;
        MobileMenu_Render = overlayRender;
        // a) normal: the stock pass reaches DrawTitleSafeArea, the PC menu is painted there, the overlay comes after it
        stockCallsTail = true; tailCallsPerFrame = 1; int t0 = tailOrig; opsBuf.clear(); Frame(false, 0, 0);
        size_t pos = opsBuf.find(" 9 9 9 9\n"); int total = Lines(opsBuf);
        CHECK(pos != std::string::npos && opsBuf.find('S') < pos);                                    // PC sprites first...
        CHECK(pos != std::string::npos && opsBuf.find('\n', pos) + 1 == opsBuf.size());              // ...the overlay is the last thing drawn
        CHECK(tailOrig == t0 + 1 && g_paintPending == false);                                         // the real DrawTitleSafeArea still ran, once
        // b) fallback: a stock pass that never reaches its end (or no tail hook) -> painted right after it, once, like before
        stockCallsTail = false; opsBuf.clear(); Frame(false, 0, 0);
        pos = opsBuf.find(" 9 9 9 9\n"); CHECK(pos != std::string::npos && Lines(opsBuf) == total && opsBuf.find('S') > pos && g_paintPending == false);
        // c) the stock pass calls it more than once: still painted once
        stockCallsTail = true; tailCallsPerFrame = 3; t0 = tailOrig; opsBuf.clear(); Frame(false, 0, 0);
        CHECK(Lines(opsBuf) == total && tailOrig == t0 + 3);
        tailCallsPerFrame = 1;
        // d) DrawTitleSafeArea called by something else (HUD, outside the menu render): passes straight through, paints nothing
        t0 = tailOrig; opsBuf.clear(); g_paintPending = true; HookOf_CWidget_DrawTitleSafeArea(); g_paintPending = false; CHECK(tailOrig == t0 + 1 && opsBuf.empty());
        // e) PC menu off: nothing is pending, nothing painted, the overlay is drawn as usual
        g_cfgEnabled = false; opsBuf.clear(); Frame(false, 0, 0); CHECK(g_paintPending == false && Lines(opsBuf) == 1 && opsBuf.find(" 9 9 9 9\n") != std::string::npos); g_cfgEnabled = true;
        // f) every screen kind goes through the same path: options page, settings page, slot list
        Idle(3); ComputeLayout(); Tap(L.cx, rootY(5)); CHECK(g_kind == SK_OPTIONS);
        opsBuf.clear(); Frame(false, 0, 0); pos = opsBuf.find(" 9 9 9 9\n"); CHECK(pos != std::string::npos && opsBuf.find('S') < pos && opsBuf.find('\n', pos) + 1 == opsBuf.size());
        ComputeLayout(); Tap(L.pOptCx, PageRowCY(1)); CHECK(g_kind == SK_PAGE);
        opsBuf.clear(); Frame(false, 0, 0); pos = opsBuf.find(" 9 9 9 9\n"); CHECK(pos != std::string::npos && opsBuf.find('S') < pos && opsBuf.find('\n', pos) + 1 == opsBuf.size());
        // g) the map view draws nothing of its own, but the overlay still shows
        Reset(); Tap(L.cx, rootY(2)); CHECK(g_view == VIEW_MAP); opsBuf.clear(); Frame(false, 0, 0); CHECK(Lines(opsBuf) == 1);
        MobileMenu_Render = fStockRender; stockCallsTail = true; tailCallsPerFrame = 1; Reset();
    }

    // =================================================================================== R8: v1.1.7 Arabic
    printf("R8  Arabic: letter forms, right-to-left order, PC menu texts, mirrored rows, Language page\n");
    {
        auto Reset = [&]() { pending = NULL; depth = 0; menuClosed = false; fakeAdj = NULL; Sync(); PushNow(rootScr); g_cfgEnabled = true; g_pcOptions = true; playing = true; g_view = VIEW_MAIN; g_cfgModsInMain = false; Idle(40); ComputeLayout(); };
        auto SetLang = [&](int l) { gameLang = l; fakeFront[OFF_FE_LANGUAGE] = (uint8_t)l; };
        auto Labels = [&]() { std::string r; for(auto& e : g_entries) r += (r.empty() ? "" : "|") + e.label; return r; };
        auto ReadFile = [&](const char* path) { std::string t; FILE* f = fopen(path, "rb"); if(f) { char b[512]; size_t n; while((n = fread(b, 1, sizeof b, f)) > 0) t.append(b, n); fclose(f); } return t; };
        auto Codes = [&](const char* utf8) { std::vector<uint16_t> v; ArShape(utf8, v); return v; };
        auto Same = [&](const std::vector<uint16_t>& a, std::initializer_list<unsigned> b) { if(a.size() != b.size()) return false; size_t i = 0; for(unsigned x : b) if(a[i++] != x) return false; return true; };
        struct Op { char t; int tex; float x0, y0, x1, y1, u0, u1; };
        auto ParseOps = [&](const std::string& txt) { std::vector<Op> v; size_t pos = 0;
            while(pos < txt.size()) { size_t e = txt.find('\n', pos); if(e == std::string::npos) e = txt.size(); std::string l = txt.substr(pos, e - pos); pos = e + 1;
                Op o = {}; float a[16];
                if(l.size() > 2 && l[0] == 'S' && sscanf(l.c_str(), "S %d %f %f %f %f %f %f %f %f %f %f %f %f", &o.tex, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8], &a[9], &a[10], &a[11]) == 13)
                { o.t = 'S'; o.x0 = a[0]; o.y0 = a[1]; o.x1 = a[6]; o.y1 = a[7]; o.u0 = a[8]; o.u1 = a[10]; v.push_back(o); }
                else if(l.size() > 2 && l[0] == 'Q' && sscanf(l.c_str(), "Q %f %f %f %f %f %f %f %f", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7]) == 8)
                { o.t = 'Q'; o.tex = 0; o.x0 = a[0]; o.y0 = a[1]; o.x1 = a[6]; o.y1 = a[7]; v.push_back(o); } }
            return v; };
        // ink of the text / ticks of one row: [min x0, max x1] of the glyph sprites (bank 1, bank_ar 5) or tick quads in the row's band
        auto Ink = [&](const std::vector<Op>& ops, float cy, float halfBand, bool quads, float xmin, float xmax, float* lo, float* hi) { bool any = false; *lo = 1e9f; *hi = -1e9f;
            for(auto& o : ops) { bool tx = (o.t == 'S' && (o.tex == 1 || o.tex == 5)); if(quads ? o.t != 'Q' : !tx) continue;
                if(o.y1 < cy - halfBand || o.y0 > cy + halfBand || o.x1 < xmin || o.x0 > xmax || (quads && (o.x1 - o.x0 > 0.2f * L.W))) continue;
                any = true; if(o.x0 < *lo) *lo = o.x0; if(o.x1 > *hi) *hi = o.x1; } return any; };
        *(void**)pDisp = vtPage; *(void**)pGame = vtPage; *(void**)pAudio = vtPage; g_cfgLang = 0; SetLang(0); *(uint8_t*)(rootScr + OFF_SCR_ISPAUSE) = 1;
        g_arOK = false; g_bank.hasAr = g_beck.hasAr = false;

        // the atlases the generator made are readable (the test replays them as textures 5 and 6)
        CHECK(g_bank.ar[ArSlot(0xFE8D)].valid && g_bank.ar[ArSlot(0xFEFB)].valid && g_bank.ar[ArSlot(0x061F)].valid && g_beck.ar[ArSlot(0xFEDF)].valid);
        { int n = 0; for(int i = 0; i < AR_GLYPHS; ++i) n += g_bank.ar[i].valid; CHECK(n == 128); }
        // every form the shaper can produce has a glyph in both atlases
        { bool all = true; for(int i = 0; i < kArLetterCount; ++i) { const uint16_t f[4] = { kArLetters[i].iso, kArLetters[i].fin, kArLetters[i].ini, kArLetters[i].med };
              for(int k = 0; k < 4; ++k) if(f[k] && !(g_bank.ar[ArSlot(f[k])].valid && g_beck.ar[ArSlot(f[k])].valid)) all = false; }
          for(auto& l : kArLigs) if(!(g_bank.ar[ArSlot(l.iso)].valid && g_bank.ar[ArSlot(l.fin)].valid)) all = false; CHECK(all); }

        // ---- shaping: joined forms, lam-alef, direction
        CHECK(Same(Codes("\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8"), { 0xFE8F, 0xFE8E, 0xFE98, 0xFEDB }));                   // kitab: beh isolated (after alef), alef final, teh medial, kaf initial; visual order = reversed
        CHECK(Same(Codes("\xD9\x83\xD9\x84\xD8\xA7"), { 0xFEFC, 0xFEDB }));                                          // kaf + lam-alef: the final ligature, kaf initial
        CHECK(Same(Codes("\xD9\x84\xD8\xA7"), { 0xFEFB }));                                                         // lam-alef alone: isolated ligature
        CHECK(Same(Codes("\xD9\x84\xD8\xA3"), { 0xFEF7 }) && Same(Codes("\xD9\x84\xD8\xA5"), { 0xFEF9 }) && Same(Codes("\xD9\x84\xD8\xA2"), { 0xFEF5 }));
        CHECK(Same(Codes("\xD8\xA7\xD9\x84"), { 0xFEDD, 0xFE8D }));                                                 // alef then lam: no ligature, both isolated
        CHECK(Same(Codes("\xD9\x85\xD9\x88\xD8\xAF"), { 0xFEA9, 0xFEEE, 0xFEE3 }));                                  // mod: meem initial, waw final (does not join on), dal isolated
        CHECK(Same(Codes("MOD"), { 'M', 'O', 'D' }) && Same(Codes("50%"), { '5', '0', '%' }) && Same(Codes(""), {}));
        CHECK(Same(Codes("MOD \xD8\xA7\xD9\x84\xD9\x85\xD9\x88\xD8\xAF"), { 0xFEA9, 0xFEEE, 0xFEE4, 0xFEDF, 0xFE8D, ' ', 'M', 'O', 'D' }));   // Latin first in the text = at the right end
        CHECK(Same(Codes("3 \xD9\x85\xD9\x88\xD8\xAF"), { 0xFEA9, 0xFEEE, 0xFEE3, ' ', '3' }));                     // a number at the start of the text is at the right
        CHECK(Same(Codes("\xD9\x85\xD9\x88\xD8\xAF 12.5%"), { '1', '2', '.', '5', '%', ' ', 0xFEA9, 0xFEEE, 0xFEE3 }));   // numbers stay together, left to right, left of the word
        CHECK(Codes("\xD9\x86\xD8\xB9\xD9\x85\xD8\x9F")[0] == 0x061F);                                               // the question mark ends the sentence: left end
        CHECK(Same(Codes("(\xD8\xA7)"), { '(', 0xFE8D, ')' }));                                                      // brackets are mirrored with the run: "(alef)" stays visually "(alef)"
        CHECK(Codes("\xD9\x85\xD9\x88\xD8\xAF\xD9\x8E") == Codes("\xD9\x85\xD9\x88\xD8\xAF"));                    // vowel signs (fatha) are dropped
        CHECK(Codes("\xD9\x83\xD9\x80\xD8\xA8") == Codes("\xD9\x83\xD8\xA8"));                                  // tatweel dropped
        CHECK(HasArabic("a\xD8\xA7") && !HasArabic("abc ?") && !HasArabic(""));
        { std::string t = "ab\xD8\xA7\xD9\x84"; Utf8Pop(t); CHECK(t == "ab\xD8\xA7"); Utf8Pop(t); Utf8Pop(t); CHECK(t == "a"); Utf8Pop(t); Utf8Pop(t); CHECK(t.empty()); }   // never half a character
        { std::vector<uint32_t> cp; Utf8ToCps(std::string("a\xD8", 2), cp); CHECK(cp.size() == 2 && cp[1] == '?'); cp.clear(); Utf8ToCps("\xD8\xA7\xFF" "b", cp); CHECK(cp.size() == 3 && cp[0] == 0x0627 && cp[1] == '?' && cp[2] == 'b'); }   // broken UTF-8 never crashes
        // no atlas: strings are measured / drawn as before (Latin '?' for what is unknown), never a crash
        { float w0 = g_bank.Width("\xD8\xA7", 1.0f, 0.0f); CHECK(w0 > 0); drawSprites = 0; DrawText(g_bank, "\xD8\xA7", 10, 100, 1.0f, 0, COL_WHITE); }

        // ---- the atlases are on: widths come from the Arabic atlas, one sprite per glyph, letter spacing only for Latin glyphs
        g_bank.texAr = (void*)5; g_beck.texAr = (void*)6; g_bank.hasAr = g_beck.hasAr = true; g_arOK = true;
        { std::string w = "\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8"; float sc = 0.7f, tr = 3.0f;
          float exp = (g_bank.ar[ArSlot(0xFE8F)].adv + g_bank.ar[ArSlot(0xFE8E)].adv + g_bank.ar[ArSlot(0xFE98)].adv + g_bank.ar[ArSlot(0xFEDB)].adv) * sc;
          CHECK(fabsf(g_bank.Width(w, sc, tr) - exp) < 0.01f);                                                   // Arabic letters touch: the letter spacing is not added
          opsBuf.clear(); DrawText(g_bank, w, 100, 200, sc, tr, COL_WHITE); auto o = ParseOps(opsBuf); CHECK(o.size() == 4 && o[0].tex == 5);
          bool joined = true; for(size_t i = 0; i + 1 < o.size(); ++i) if(o[i + 1].x0 > o[i].x1 + 0.5f) joined = false; CHECK(joined);   // neighbouring glyph boxes meet / overlap: no gaps in a joined word
          float wm = g_bank.Width("MOD", sc, tr); CHECK(fabsf(wm - ((g_bank.Get('M').adv + g_bank.Get('O').adv + g_bank.Get('D').adv) * sc * g_bank.xs + 2 * tr)) < 0.01f);   // Latin: unchanged
          std::string mixed = "MOD " + w; opsBuf.clear(); DrawText(g_bank, mixed, 50, 200, sc, tr, COL_WHITE); o = ParseOps(opsBuf);
          CHECK(o.size() == 4 + 3 * 2 && o[0].tex == 5 && o[3].tex == 5 && o[4].tex == 1 && o[9].tex == 1);          // 4 Arabic glyphs, then M O D (each drawn twice: the PC font is synthetically bold)                                           // Arabic left, Latin MOD at the right end
          std::string fit = Fit(g_bank, "\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7 \xD8\xA8\xD9\x83 \xD9\x81\xD9\x8A \xD9\x82\xD8\xA7\xD8\xA6\xD9\x85\xD8\xA9", sc, tr, 120); CHECK(fit.size() > 3 && fit.compare(fit.size() - 3, 3, "...") == 0 && g_bank.Width(fit, sc, tr) <= 120 + 0.01f); }

        // ---- the PC menu in Arabic
        g_cfgLang = LG_AR; SetLang(0); Reset(); CHECK(UiLang() == LG_AR && IsRTL());
        CHECK(Labels() == "متابعة|بدء لعبة جديدة|الخريطة|الإحصائيات|الملخص|الخيارات|الخروج من اللعبة"); Shot("16_ar_pause");
        CHECK(TitleOf("PAUSE MENU") == "قائمة الإيقاف المؤقت" && TitleOf("MAIN MENU") == "القائمة الرئيسية" && OptionsTitle() == "الخيارات" && Tr("BACK") == "رجوع" && Tr("PLEASE WAIT") == "يرجى الانتظار");
        *(uint8_t*)(rootScr + OFF_SCR_ISPAUSE) = 0; Idle(3); CHECK(Labels() == "متابعة|بدء اللعبة|الخيارات|الخروج من اللعبة"); *(uint8_t*)(rootScr + OFF_SCR_ISPAUSE) = 1; Idle(3);
        { opsBuf.clear(); Frame(false, 0, 0); auto o = ParseOps(opsBuf); bool flipped = false, hasAr = false; for(auto& q : o) { if(q.tex == 3 && q.u0 > q.u1) flipped = true; if(q.tex == 5) hasAr = true; }
          CHECK(flipped && hasAr); }                                                                              // the cursor arrow is mirrored (right of the text), glyphs come from the Arabic atlas
        // the title is drawn from the Arabic title atlas
        { opsBuf.clear(); Frame(false, 0, 0); auto o = ParseOps(opsBuf); bool t6 = false; for(auto& q : o) if(q.tex == 6) t6 = true; CHECK(t6); }
        // other languages untouched: the cursor is not mirrored
        g_cfgLang = 0; Idle(3); { opsBuf.clear(); Frame(false, 0, 0); auto o = ParseOps(opsBuf); bool flipped = false, any5 = false; for(auto& q : o) { if(q.tex == 3 && q.u0 > q.u1) flipped = true; if(q.tex == 5 || q.tex == 6) any5 = true; } CHECK(!flipped && !any5); }
        g_cfgLang = LG_AR; Idle(3);
        // Options tiles, Display page
        ComputeLayout(); Tap(L.cx, rootY(5)); CHECK(g_kind == SK_OPTIONS && g_optRows.size() == 6);
        CHECK(g_optRows[0].title == "اللعبة" && g_optRows[1].title == "العرض" && g_optRows[2].title == "الصوت" && g_optRows[3].title == "إعدادات المودات"); Shot("17_ar_options");
        ComputeLayout(); Tap(L.pOptCx, PageRowCY(1));
        CHECK(g_kind == SK_PAGE && g_pageTitle == "العرض" && g_pageBg == 3);
        Idle(2);
        CHECK(g_pageRows.size() == 4 && g_pageRows[0].label == "المؤثرات البصرية" && g_pageRows[1].label == "المؤثرات الصوتية" && g_pageRows[2].label == "عرض معدل الإطارات" && g_pageRows[3].label == "استعادة الإعدادات الافتراضية");
        CHECK(g_pageRows[2].valText == "إيقاف"); g_sel = 1; Shot("18_ar_display");
        // mirrored rows: labels end at the right margin, toggles / bars sit at the left, the slider grows from the right
        { ComputeLayout(); std::vector<Op> o; float lo, hi; opsBuf.clear(); Frame(false, 0, 0); o = ParseOps(opsBuf); float step = PageStep();
          CHECK(Ink(o, PageRowCY(0), step * 0.45f, false, 0, L.W, &lo, &hi) && fabsf(hi - (L.W - L.pLabelX)) < 0.012f * L.W);                   // label of row 0: right edge at the right margin
          CHECK(Ink(o, PageRowCY(2), step * 0.45f, false, 0, 0.3f * L.W, &lo, &hi) && fabsf(lo - (L.W - L.pValueR)) < 0.012f * L.W);          // toggle value of row 2: left edge at the left margin
          CHECK(Ink(o, PageRowCY(1), step * 0.45f, true, 0, L.W, &lo, &hi) && fabsf(lo - (L.W - L.pValueR)) < 1.0f && fabsf(hi - (L.W - L.pValueR + L.pBarW)) < 1.0f);   // ticks of the slider: left margin ... + bar width
          CHECK(Ink(o, PageRowCY(1), step * 0.45f, false, 0, 0.5f * L.W, &lo, &hi) && lo >= L.W - L.pValueR + L.pBarW - 1.0f);                  // its percentage: right of the bar
          // slider: the right end is the minimum, the left end the maximum
          float barL = L.W - L.pValueR, barR = barL + L.pBarW;
          Tap(barL - 4, PageRowCY(1)); Idle(2); SView v; ReadSetting(12, v); CHECK(v.value == 80);
          Tap(barR + 4, PageRowCY(1)); Idle(2); ReadSetting(12, v); CHECK(v.value == 0);
          Tap((barL + barR) * 0.5f, PageRowCY(1)); Idle(2); ReadSetting(12, v); CHECK(v.value >= 38 && v.value <= 42);
          // dragging along the bar
          Frame(true, barR - 4, PageRowCY(1)); Frame(true, barL + 0.75f * L.pBarW, PageRowCY(1)); Frame(false, barL + 0.75f * L.pBarW, PageRowCY(1)); Idle(3); ReadSetting(12, v); CHECK(v.value >= 18 && v.value <= 22);
          WriteSetting(12, 40);
          // left of the bar (where the left-to-right layout has its label) is just a row: a tap there toggles / activates, the slider keeps its value
          ReadSetting(12, v); int before = v.value; CHECK(before == 40); Tap(L.W * 0.6f, PageRowCY(1)); Idle(2); ReadSetting(12, v); CHECK(v.value == before);
          // the scroll bar moves to the left edge
          int sbBefore = 0; (void)sbBefore; }
        // left to right again: nothing moved
        g_cfgLang = 0; Idle(3); { std::vector<Op> o; float lo, hi; opsBuf.clear(); Frame(false, 0, 0); o = ParseOps(opsBuf); float step = PageStep();
          CHECK(Ink(o, PageRowCY(0), step * 0.45f, false, 0, L.W, &lo, &hi) && fabsf(lo - L.pLabelX) < 0.012f * L.W);
          CHECK(Ink(o, PageRowCY(2), step * 0.45f, false, 0.6f * L.W, L.W, &lo, &hi) && fabsf(hi - L.pValueR) < 0.012f * L.W);
          CHECK(Ink(o, PageRowCY(1), step * 0.45f, true, 0, L.W, &lo, &hi) && fabsf(hi - L.pValueR) < 1.0f && fabsf(lo - (L.pValueR - L.pBarW)) < 1.0f);
          float barL = L.pValueR - L.pBarW; Tap(barL - 4, PageRowCY(1)); Idle(2); SView v; ReadSetting(12, v); CHECK(v.value == 0); Tap(L.pValueR + 4, PageRowCY(1)); Idle(2); ReadSetting(12, v); CHECK(v.value == 80); WriteSetting(12, 40); }
        g_cfgLang = LG_AR; Idle(3);
        // toggle value, ACTIVE row text, dialog text
        Tap(L.W - L.pLabelX - 60, PageRowCY(2)); Idle(2); CHECK(g_pageRows[2].valText == "تشغيل"); WriteSetting(3, 0);
        CHECK(Resolve("FESZ_QL") == "هل أنت متأكد أنك تريد تحميل هذه اللعبة؟" && Resolve("FESZ_QL", false) == "Are you sure you want to load this game?");
        CHECK(Resolve("FEM_YES") == "نعم" && Resolve("FEM_NO") == "لا" && Resolve("Unknown Mod Text") == "Unknown Mod Text");
        CHECK(PhraseFor(LG_AR, "AUTOSAVE: EMPTY SLOT") == "حفظ تلقائي: خانة فارغة" && PhraseFor(LG_AR, "Brightness") == "السطوع" && PhraseFor(LG_AR, "nothing known here").empty());
        CHECK(PhraseFor(LG_AR, "Are you sure?") == "هل أنت متأكد؟" && PhraseFor(LG_ID, "Are you sure?") == "APAKAH ANDA YAKIN?");
        // save slots, Start Game tiles (left aligned in the left-to-right layout: right aligned now) and the confirm dialog
        Reset(); Tap(L.cx, rootY(1)); ComputeLayout(); CHECK(g_kind == SK_START && g_optRows.size() == 3 && g_optRows[0].label == "لعبة جديدة" && g_optRows[1].label == "تحميل لعبة" && g_optRows[2].label == "حذف لعبة");
        Shot("23_ar_startgame");
        { std::vector<Op> o; float lo, hi; opsBuf.clear(); Frame(false, 0, 0); o = ParseOps(opsBuf); CHECK(Ink(o, PageRowCY(1), PageStep() * 0.45f, false, 0, L.W, &lo, &hi) && fabsf(hi - (L.W - L.sLabelX)) < 0.012f * L.W); }
        Tap(L.W - L.sLabelX - 60, PageRowCY(1)); Idle(3);
        CHECK(g_kind == SK_PAGE && g_style == PS_SLOTS && g_pageRows.size() == 9);
        Idle(40); CHECK(g_pageRows[0].label == "حفظ تلقائي: خانة فارغة" && g_pageRows[1].label == "BMX" && g_pageRows[3].label == "خانة فارغة" && g_pageRows[7].label == "السحابة غير متاحة");
        Shot("20_ar_slots");
        { std::vector<Op> o; float lo, hi; opsBuf.clear(); Frame(false, 0, 0); o = ParseOps(opsBuf); CHECK(Ink(o, PageRowCY(1), PageStep() * 0.45f, false, 0, L.W, &lo, &hi) && fabsf(hi - (L.W - L.sLabelX)) < 0.012f * L.W); }   // "BMX" (Latin) also ends at the right margin
        Tap(L.W - L.sLabelX - 40, PageRowCY(1)); Idle(14); CHECK(g_kind == SK_PAGE && g_style == PS_DIALOG && g_dlgMsg == "هل أنت متأكد أنك تريد تحميل هذه اللعبة؟" && g_pageRows.size() == 2);
        Shot("21_ar_dialog");
        // the Mods list
        Reset(); g_cfgModsInMain = true; Idle(3); CHECK(g_entries.size() == 8 && g_entries[6].label == "المودات"); ComputeLayout(); Tap(L.cx, rootY(6)); CHECK(g_view == VIEW_MODS && g_modRows[0].label == "إعدادات المودات");
        Shot("19_ar_mods"); g_cfgModsInMain = false;

        // ---- the Language page: two extra rows now (Indonesian, Arabic)
        alignas(8) static uint8_t pLang[256]; static void* vtLang[16]; static void* langItems[6];
        for(int i = 0; i < 16; ++i) vtLang[i] = (void*)0x1000;
        vtLang[9] = (void*)fOnBack; vtLang[11] = (void*)fFlowNo;
        static void (*pick)(void*, int) = [](void*, int d) { gameLang = d; fakeFront[OFF_FE_LANGUAGE] = (uint8_t)d; calls.push_back("lang" + std::to_string(d)); };
        langItems[0] = MkAction("FEL_ENG", pick, 0); langItems[1] = MkAction("FEL_FRE", pick, 1); langItems[2] = MkAction("FEL_RUS", pick, 5);
        *(void**)pLang = vtLang; *(int*)(pLang + SEL_ITEMCOUNT) = 3; *(void***)(pLang + SEL_ITEMS) = langItems;
        g_vtLanguage = (uintptr_t)vtLang; g_selVts[g_selVtCount] = (uintptr_t)vtLang; g_selStyle[g_selVtCount] = PS_SETTINGS; g_selTitle[g_selVtCount] = ""; ++g_selVtCount;
        std::string keepDir = g_dirPC; mkdir("/tmp/pcm_lang", 0777); g_dirPC = "/tmp/pcm_lang/";
        { FILE* f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs("Scale=1.0\r\nLanguage=Auto\r\nSounds=0\r\n", f); fclose(f); }
        calls.clear(); g_cfgLang = 0; SetLang(0); Reset(); Tap(L.cx, rootY(5)); Idle(5); Push(pLang); Idle(40); ComputeLayout();
        CHECK(g_kind == SK_PAGE && g_activeVt == g_vtLanguage);
        CHECK(g_pageRows.size() == 5 && g_pageRows[3].kind == 4 && g_pageRows[3].data == LG_ID && g_pageRows[3].label == "BAHASA INDONESIA" && g_pageRows[4].kind == 4 && g_pageRows[4].data == LG_AR && g_pageRows[4].label == "العربية");
        Tap(L.pLabelX + 60, PageRowCY(4)); Idle(3);
        CHECK(g_cfgLang == LG_AR && UiLang() == LG_AR && IsRTL());
        { std::string ini = ReadFile("/tmp/pcm_lang/pcmenu.ini"); CHECK(ini.find("Language=Arabic") != std::string::npos && ini.find("Language=Auto") == std::string::npos && ini.find("Scale=1.0") != std::string::npos && ini.find("Sounds=0") != std::string::npos); }
        g_pageRowsTop = NULL; Idle(40); CHECK(g_pageRows.size() == 5 && g_pageRows[4].label == "العربية" && g_pageTitle == "اللغة");
        Shot("22_ar_language");
        { std::vector<Op> o; float lo, hi; opsBuf.clear(); Frame(false, 0, 0); o = ParseOps(opsBuf); CHECK(Ink(o, PageRowCY(4), PageStep() * 0.45f, false, 0, 0.3f * L.W, &lo, &hi) && fabsf(lo - (L.W - L.pValueR)) < 0.012f * L.W); }   // "نشط" (active) at the left margin of the Arabic row
        Tap(L.W - L.pLabelX - 60, PageRowCY(3)); Idle(3); CHECK(g_cfgLang == LG_ID && UiLang() == LG_ID);                       // Indonesian replaces Arabic
        { std::string ini = ReadFile("/tmp/pcm_lang/pcmenu.ini"); CHECK(ini.find("Language=Indonesian") != std::string::npos); }
        g_pageRowsTop = NULL; Idle(40); Tap(L.pLabelX + 60, PageRowCY(4)); Idle(3); CHECK(g_cfgLang == LG_AR);
        g_pageRowsTop = NULL; Idle(40); Tap(L.W - L.pLabelX - 60, PageRowCY(1)); Idle(3);                                         // FRENCH: the stock action runs, the override is cleared and saved
        CHECK(g_cfgLang == 0 && gameLang == 1 && UiLang() == LG_FR && std::count(calls.begin(), calls.end(), "lang1") == 1);
        { std::string ini = ReadFile("/tmp/pcm_lang/pcmenu.ini"); CHECK(ini.find("Language=Auto") != std::string::npos && ini.find("Arabic") == std::string::npos); }
        // without the Arabic atlases the row is not offered and a saved "Arabic" choice is ignored
        g_arOK = false; g_bank.hasAr = g_beck.hasAr = false; g_cfgLang = LG_AR; g_pageRowsTop = NULL; Idle(40);
        CHECK(UiLang() == LG_FR && !IsRTL() && g_pageRows.size() == 4 && g_pageRows[3].data == LG_ID);
        { opsBuf.clear(); Frame(false, 0, 0); auto o = ParseOps(opsBuf); bool any5 = false; for(auto& q : o) if(q.tex == 5 || q.tex == 6) any5 = true; CHECK(!any5); }
        g_arOK = true; g_bank.hasAr = g_beck.hasAr = true;
        // ini round trips
        { remove("/tmp/pcm_lang/pcmenu.ini"); g_cfgLang = LG_AR; SaveLanguageSetting(); CHECK(ReadFile("/tmp/pcm_lang/pcmenu.ini") == "Language=Arabic\n");
          g_cfgLang = 0; LoadConfig(); CHECK(g_cfgLang == LG_AR);
          const char* ok[] = { "Language=Arabic", "Language=ARABIC", "Language = ar", "Language=\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A\xD8\xA9" };
          for(const char* l : ok) { FILE* f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs(l, f); fputs("\n", f); fclose(f); g_cfgLang = 0; LoadConfig(); CHECK(g_cfgLang == LG_AR); }
          FILE* f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs("Language=English\n", f); fclose(f); g_cfgLang = LG_AR; LoadConfig(); CHECK(g_cfgLang == 0);
          f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs("Scale=1.2\nLanguage=Indonesian\nSounds=1", f); fclose(f); g_cfgLang = LG_AR; SaveLanguageSetting(); CHECK(ReadFile("/tmp/pcm_lang/pcmenu.ini") == "Scale=1.2\nLanguage=Arabic\nSounds=1"); }
        g_dirPC = keepDir; g_cfgLang = 0; SetLang(0); --g_selVtCount; g_vtLanguage = 0; Reset();
        CHECK(Labels() == "RESUME|START NEW GAME|MAP|STATS|BRIEF|OPTIONS|QUIT GAME");

        // ---- every Arabic phrase of the tables: only letters the atlas knows, no '?', widths sane; dumped for the pixel comparison against a real text shaper (tools/check_arabic.py)
        { FILE* dump = fopen("/tmp/pcm_ar_codes.txt", "w"); int n = 0; bool clean = true;
          auto One = [&](const char* ar) { std::vector<uint16_t> c = Codes(ar); ++n; for(uint16_t x : c) { if(x == '?') clean = false; if(x >= 128 && (ArSlot(x) < 0 || !g_bank.ar[ArSlot(x)].valid)) clean = false; }
              if(dump) { fprintf(dump, "%s\t", ar); for(uint16_t x : c) fprintf(dump, "%u ", (unsigned)x); fprintf(dump, "\n"); } };
          for(int i = 0; i < kTxCount; ++i) { CHECK(kTx[i].ar && kTx[i].ar[0]); One(kTx[i].ar); }
          for(int i = 0; i < kIdPhraseCount; ++i) { CHECK(kIdPhrases[i].ar && kIdPhrases[i].ar[0]); One(kIdPhrases[i].ar); }
          for(size_t i = 0; i < sizeof(kIdTokens) / sizeof(kIdTokens[0]); ++i) One(kIdTokens[i].ar);
          One("\xD9\x86\xD8\xB9\xD9\x85\xD8\x9F"); One("MOD \xD8\xA7\xD9\x84\xD9\x85\xD9\x88\xD8\xAF"); One("\xD9\x84\xD8\xA7 \xD8\xAA\xD9\x88\xD8\xAC\xD8\xAF \xD8\xA5\xD8\xB9\xD8\xAF\xD8\xA7\xD8\xAF\xD8\xA7\xD8\xAA \xD9\x84\xD9\x84\xD9\x85\xD9\x88\xD8\xAF SAUTILS - \xD8\xB9\xD8\xB1\xD8\xB6 \xD8\xA7\xD9\x84\xD9\x83\xD9\x84");
          if(dump) fclose(dump); CHECK(clean); CHECK(n > 130); }
        // a toast / header that mixes Arabic with a mod name and a number
        g_cfgLang = LG_AR; { std::string t = Tr("NO SETTINGS FOUND FOR") + " " + Upper("saUtils") + " - " + Tr("SHOWING ALL"); std::vector<uint16_t> c = Codes(t.c_str()); CHECK(c.size() > 20);
          size_t at = 0; for(size_t i = 0; i < c.size(); ++i) if(c[i] == 'S') { at = i; break; } CHECK(at > 0 && at + 6 < c.size() && c[at + 1] == 'A' && c[at + 2] == 'U' && c[at + 6] == 'S' && c[at - 1] == ' '); }   // the name reads left to right inside the sentence
        g_cfgLang = 0; Reset();
    }

    // =================================================================================== R9: one click per tap (v1.1.7)
#ifndef OLD_SOURCE
    printf("R9  sounds: one click per tap, however many clicks the stock code asks for (device report: 2 to 4 clicks for one tap)\n");
    {
        g_cfgSounds = true; g_sndSelect.ok = g_sndBack.ok = true; g_sndSelect.bq = &bqSelP; g_sndBack.bq = &bqBackP; g_sndSelect.pcm.assign(64, 0); g_sndBack.pcm.assign(64, 0);
        auto Clear0 = [&]() { sndSel = sndBack = 0; memset(engEv, 0, sizeof engEv); };
        auto Heard = [&]() { return sndSel + sndBack + engEv[1] + engEv[2] + engEv[3] + engEv[4]; };     // everything the user would hear: our clips + every game click
        g_cfgPCOptions = true; g_cfgModsInMain = false; playing = true; g_cfgLang = 0; gameLang = 0;

        // ---- main menu: tap on a row that is not highlighted yet used to click on finger-down AND on release (+ the stock tile click, + the screen-change sound)
        ResetToRoot(); g_sel = 0; Clear0(); Tap(L.cx, rootY(5));                         // OPTIONS: tile callback clicks, the screen change sounds
        CHECK(depth >= 2); CHECK(sndSel == 1 && sndBack == 0); CHECK(engEv[1] == 0 && engEv[2] == 0 && engEv[3] == 0 && engEv[4] == 0); CHECK(Heard() == 1);
        ResetToRoot(); Clear0(); Tap(L.cx, rootY(0)); CHECK(Heard() == 1 && sndSel == 1);                   // RESUME (a highlighted row: the old code clicked once here)
        Tap(L.cx, rootY(0)); CHECK(Heard() == 2 && sndSel == 2);                                           // the same row again = one more click, not two
        Tap(L.cx, rootY(6)); CHECK(Heard() == 3 && sndSel == 3);                                           // another row: still one
        // a finger that only touches, or that wanders away before lifting, makes no sound at all
        Clear0(); Frame(true, L.cx, rootY(3)); Frame(true, L.cx, rootY(3)); Frame(true, L.cx, rootY(3)); CHECK(Heard() == 0);
        Frame(true, L.cx + L.W * 0.35f, rootY(3)); Frame(false, 0, 0); Idle(12); CHECK(Heard() == 0);
        // refused (no running game): the back clip, once
        playing = false; Clear0(); Tap(L.cx, rootY(3)); CHECK(sndBack == 1 && sndSel == 0 && Heard() == 1); playing = true;

        // ---- Options -> a settings page: tile, toggle, slider, BACK (button and Android key)
        ResetToRoot(); Tap(L.cx, rootY(5)); ComputeLayout(); Clear0();
        Tap(L.pOptCx, PageRowCY(1)); CHECK(g_kind == SK_PAGE); CHECK(Heard() == 1 && sndSel == 1);          // the Display tile
        Clear0(); Tap(L.pLabelX + 60, PageRowCY(0)); CHECK(Heard() == 1 && sndSel == 1);                     // a toggle row
        Clear0(); { float bl = L.pValueR - L.pBarW; Frame(true, bl + L.pBarW * 0.5f, PageRowCY(1)); Frame(true, bl + L.pBarW * 0.6f, PageRowCY(1)); Frame(false, 0, 0); Idle(12); }
        CHECK(Heard() == 0);                                                                              // a slider is dragged silently
        Clear0(); Tap(L.pBackL + 30, L.pBackT + 20); CHECK(depth == 2 && Heard() == 1 && sndBack == 1 && sndSel == 0);   // BACK: our clip only (the stock OnBack asks for one too, and the screen change)
        Tap(L.pOptCx, PageRowCY(0)); CHECK(g_kind == SK_PAGE); Clear0();
        fakeEscape = true; Frame(false, 0, 0); fakeEscape = false; Idle(40); CHECK(depth == 2 && Heard() == 1 && sndBack == 1);   // the Android back key
        Clear0(); Tap(L.pBackL + 30, L.pBackT + 20); Idle(30); CHECK(depth == 1 && Heard() == 1 && sndBack == 1);

        // ---- Start Game -> Load Game -> a slot row (the stock HandleInput clicks by itself: swallowed, replayed once as ours), a row that does nothing stays silent
        ResetToRoot(); Clear0(); Tap(L.cx, rootY(1)); CHECK(g_kind == SK_START && Heard() == 1);
        ComputeLayout(); Clear0(); Tap(L.sLabelX + 60, PageRowCY(1)); CHECK(g_kind == SK_PAGE && g_style == PS_SLOTS && Heard() == 1);
        ComputeLayout(); Clear0(); slotSilent = false; Tap(L.sLabelX + 60, PageRowCY(1)); CHECK(g_style == PS_DIALOG); CHECK(sndSel == 1 && Heard() == 1 && engEv[1] == 0);
        ComputeLayout(); Clear0(); Tap(L.cx, PageRowCY(1)); CHECK(g_style == PS_SLOTS && Heard() == 1);        // NO in the dialog
        ComputeLayout(); Clear0(); slotSilent = true; Tap(L.sLabelX + 60, PageRowCY(2)); CHECK(Heard() == 0);  // the stock row made no click: neither do we
        slotSilent = false;

        // ---- the map: the stock map and its Back button own the finger, their ONE click passes; a repeat of it is dropped
        ResetToRoot(); Clear0(); Tap(L.cx, rootY(2)); CHECK(g_view == VIEW_MAP && Heard() == 1 && sndSel == 1);      // opening the map: our click
        Clear0(); StockBackTap(); Idle(20); CHECK(g_view == VIEW_MAIN); CHECK(engEv[2] == 1 && sndSel == 0 && sndBack == 0 && Heard() == 1);
        Clear0(); Tap(L.cx, rootY(2)); CHECK(g_view == VIEW_MAP); Clear0(); mapBackDouble = true; StockBackTap(); Idle(20); mapBackDouble = false;
        CHECK(g_view == VIEW_MAIN && engEv[2] == 1 && Heard() == 1);                                        // the stock asked twice within a few ms: one click
        Tap(L.cx, rootY(2)); CHECK(g_view == VIEW_MAP); Clear0(); fakeEscape = true; Frame(false, 0, 0); fakeEscape = false; Idle(20);
        CHECK(g_view == VIEW_MAIN && sndBack == 1 && Heard() == 1);                                         // the Android back key out of the map: our back clip, once
        Idle(40); CHECK(Heard() == 1);                                                                      // and nothing trails after it

        // ---- the gate itself: a second request within 150 ms is the same click, a clip is never cut and restarted
        fakeNow += 1000; Clear0(); SndPlay(g_sndSelect); SndPlay(g_sndBack); SndPlay(g_sndSelect); CHECK(sndSel == 1 && sndBack == 0);
        fakeNow += 100; SndPlay(g_sndSelect); CHECK(sndSel == 1);
        fakeNow += 100; SndPlay(g_sndSelect); CHECK(sndSel == 2);

        // ---- the game's clicks are only held back while the PC menu owns the screen AND plays its own sounds
        ResetToRoot(); Idle(5);
        g_cfgSounds = false; Clear0(); Tap(L.cx, rootY(5)); CHECK(sndSel == 0 && engEv[1] == 1);              // Sounds=0: the game's click is all there is
        g_cfgSounds = true; g_sndBack.ok = false; ResetToRoot(); Clear0(); Tap(L.cx, rootY(5)); CHECK(sndSel == 1 && engEv[1] == 1);   // (a clip that failed to load: do not leave the menu silent)
        g_sndBack.ok = true;
        { int keep = g_kind; g_kind = SK_NONE; Clear0(); StockClick(1); StockClick(1); StockClick(2); StockClick(3); CHECK(engEv[1] == 2 && engEv[2] == 1 && engEv[3] == 1); g_kind = keep; }   // other screens: untouched
        g_cfgSounds = false; ResetToRoot(); fakeNow += 1000; Idle(1); Clear0(); StockClick(2); StockClick(2); CHECK(engEv[2] == 1); fakeNow += 200; Idle(1); StockClick(2); CHECK(engEv[2] == 2);   // what passes is debounced too
        { Clear0(); StockClick(0x23); StockClick(0x23); CHECK(engEv[0] == 0); }                                // (other events are not ours to judge: they are never touched)
        g_cfgSounds = true; ResetToRoot(); Idle(2); Clear0(); fakeNow += 2000;                              // the menu has been closed for two seconds (g_kind is stale): the sound the game makes when the pause menu OPENS is its own
        StockClick(3); CHECK(engEv[3] == 1);
        Idle(1); Clear0(); StockClick(3); StockClick(1); CHECK(engEv[3] == 0 && engEv[1] == 0);               // ... but once the PC menu is up again the game's clicks are swallowed
        CHECK(g_feDropped > 0);
        g_cfgSounds = false; g_sndSelect.ok = g_sndBack.ok = false; ResetToRoot();
    }

    // =================================================================================== R10: v1.1.8
    printf("R10 v1.1.8: no '?' in Russian / Japanese, real tile keys, Indonesian wording, Arabic labels / size / BACK, ArabicMirror\n");
    {
        auto Reset = [&]() { pending = NULL; depth = 0; menuClosed = false; fakeAdj = NULL; Sync(); PushNow(rootScr); g_cfgEnabled = true; g_pcOptions = true; playing = true; g_view = VIEW_MAIN; g_cfgModsInMain = false; Idle(40); ComputeLayout(); };
        auto SetLang = [&](int l) { gameLang = l; fakeFront[OFF_FE_LANGUAGE] = (uint8_t)l; };
        auto Labels = [&]() { std::string r; for(auto& e : g_entries) r += (r.empty() ? "" : "|") + e.label; return r; };
        std::vector<void*> r10Mem; auto MkT = [&](void* p) { r10Mem.push_back(p); return p; };
        auto NoQ = [](const std::string& t) { return t.find('?') == std::string::npos; };
        struct Op { char t; int tex; float x0, y0, x1, y1, u0, u1; };
        auto ParseOps = [&](const std::string& txt) { std::vector<Op> v; size_t pos = 0;
            while(pos < txt.size()) { size_t e = txt.find('\n', pos); if(e == std::string::npos) e = txt.size(); std::string l = txt.substr(pos, e - pos); pos = e + 1;
                Op o = {}; float a[16];
                if(l.size() > 2 && l[0] == 'S' && sscanf(l.c_str(), "S %d %f %f %f %f %f %f %f %f %f %f %f %f", &o.tex, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8], &a[9], &a[10], &a[11]) == 13)
                { o.t = 'S'; o.x0 = a[0]; o.y0 = a[1]; o.x1 = a[6]; o.y1 = a[7]; o.u0 = a[8]; o.u1 = a[10]; v.push_back(o); } }
            return v; };
        auto Ink = [&](const std::vector<Op>& ops, int tex, float cy, float halfBand, float* lo, float* hi) { bool any = false; *lo = 1e9f; *hi = -1e9f;
            for(auto& o : ops) { if(o.tex != tex || o.y1 < cy - halfBand || o.y0 > cy + halfBand) continue; any = true; if(o.x0 < *lo) *lo = o.x0; if(o.x1 > *hi) *hi = o.x1; } return any; };
        *(void**)pDisp = vtPage; *(void**)pGame = vtPage; *(void**)pAudio = vtPage; g_cfgLang = 0; SetLang(0); *(uint8_t*)(rootScr + OFF_SCR_ISPAUSE) = 1;
        g_bank.texAr = (void*)5; g_beck.texAr = (void*)6; g_arOK = g_bank.hasAr && g_beck.hasAr;

        // ---- 1) the game's code page: Latin accents only for the Latin games; Cyrillic / kana are dropped, never '?'
        { bool ok = true; unsigned short w[] = { 0x80, 0x97, 'X', 0 };
          SetLang(0); CHECK(GxtToAscii(w, &ok) == "AaX" && ok);
          SetLang(1); CHECK(GxtToAscii(w, &ok) == "AaX" && ok);
          SetLang(5); CHECK(GxtToAscii(w, &ok) == "X" && !ok);
          SetLang(6); CHECK(GxtToAscii(w, &ok) == "X" && !ok);
          unsigned short k[] = { 0x3042, 0x3044, 0 }; CHECK(GxtToAscii(k, &ok).empty() && !ok);
          SetLang(0);
          CHECK(DrawablePart("FPS (/)") == "FPS" && DrawablePart("(/)").empty() && DrawablePart("ab").empty() && DrawablePart("Hi 5 Fps") == "Hi 5 Fps"); }

        // ---- 2) Russian and Japanese: the real tile / row keys have English names, nothing is a question mark
        for(int lg = 5; lg <= 6; ++lg)
        {
            SetLang(lg); g_cfgLang = 0; Idle(3);
            CHECK(Resolve("FEH_LOA") == "GAME" && Resolve("FEC_RED") == "CONTROLS" && Resolve("FEO_DIS") == "DISPLAY" && Resolve("FEO_AUD") == "AUDIO" && Resolve("FEO_LAN") == "LANGUAGE");
            CHECK(Resolve("FED_BRI") == "BRIGHTNESS" && Resolve("MOB_RTD") == "RESTORE DEFAULTS" && Resolve("FEM_YES") == "YES");        // the guess table
            CHECK(Resolve("FEL_ENG") == "ENGLISH" && Resolve("FEL_FRE") == "FRENCH" && Resolve("FEL_GER") == "GERMAN" && Resolve("FEL_JPN") == "JAPANESE" && Resolve("FEL_RUS") == "RUSSIAN");
            bool dr = true; std::string u = Resolve("FEX_UNK", true, &dr); CHECK(u.empty() && !dr);                                      // unknown key, undrawable text: the caller labels it
            dr = false; CHECK(Resolve("FEH_LOA", true, &dr) == "GAME" && dr);
            CHECK(Resolve("Unknown Mod Text") == "Unknown Mod Text");                                                                    // a mod's own text is untouched
            Reset(); CHECK(Labels() == "RESUME|START NEW GAME|MAP|STATS|BRIEF|OPTIONS|QUIT GAME" && NoQ(Labels()));
            Tap(L.cx, rootY(5)); CHECK(g_kind == SK_OPTIONS && g_optRows.size() == 6);
            CHECK(g_optRows[0].title == "Game" && g_optRows[1].title == "Display" && g_optRows[2].title == "Audio" && g_optRows[3].title == "Mods settings");
            { bool q = false; for(auto& r : g_optRows) if(!NoQ(r.label) || r.label.empty()) q = true; CHECK(!q); }
            if(lg == 5) Shot("25_ru_options");
            // an Options tile nobody knows: "Option 5", not "????"
            optItems[4].key = "FEX_UNK"; g_optCache.top = NULL; g_optRowsGen = 0xFFFFFFFFu; Idle(3);
            CHECK(g_optRows.size() == 6 && g_optRows[4].title == "Option 5" && g_optRows[4].label == "OPTION 5"); optItems[4].key = "ModTabA"; g_optCache.top = NULL; g_optRowsGen = 0xFFFFFFFFu; Idle(3);
            // the Game tile of the real game (FEH_LOA) opens a page with the Game picture
            ComputeLayout(); Tap(L.pOptCx, PageRowCY(0)); CHECK(g_kind == SK_PAGE && g_pageTitle == "Game" && g_pageBg == 4);
            // a settings page: known keys have English names, an unknown one is "OPTION n"
            Tap(L.pBackL + 30, L.pBackT + 20); Idle(40); CHECK(g_kind == SK_OPTIONS);
            { void* keep0 = dispItems[0]; void* keep1 = dispItems[1]; void* keep2 = dispItems[2]; void* keep3 = dispItems[3]; void* keep4 = dispItems[4];
              dispItems[0] = MkT(MkSetting("FED_BRI", 5)); dispItems[1] = MkT(MkSetting("FEX_UNK", 12)); dispItems[2] = MkT(MkAction("MOB_RTD", fRestore, 7)); SetupPage(pDisp, dispItems, 3);
              ComputeLayout(); Tap(L.pOptCx, PageRowCY(1)); g_pageRowsTop = NULL; Idle(3);
              CHECK(g_kind == SK_PAGE && g_pageRows.size() == 3 && g_pageRows[0].label == "BRIGHTNESS" && g_pageRows[1].label == "OPTION 2" && g_pageRows[2].label == "RESTORE DEFAULTS");
              { bool q = false; for(auto& r : g_pageRows) if(!NoQ(r.label)) q = true; CHECK(!q); }
              dispItems[0] = keep0; dispItems[1] = keep1; dispItems[2] = keep2; dispItems[3] = keep3; dispItems[4] = keep4; SetupPage(pDisp, dispItems, 5); g_pageRowsTop = NULL; }
            // the slot list: a save name the font cannot draw becomes "SLOT n"
            Reset(); calls.clear(); Tap(L.cx, rootY(1)); ComputeLayout();
            CHECK(g_kind == SK_START && g_optRows.size() == 3 && g_optRows[1].label == "LOAD GAME" && g_optRows[2].label == "DELETE GAME" && g_optRows[0].label == "OPTION 1");
            { const char* keepName = slotNames[3]; slotNames[3] = "\xC0\xC1"; Tap(L.sLabelX + 60, PageRowCY(1)); Idle(40);
              CHECK(g_kind == SK_PAGE && g_style == PS_SLOTS && g_pageRows.size() == 9 && g_pageRows[3].label == "SLOT 4" && g_pageRows[1].label == "BMX");
              slotNames[3] = keepName; }
            Reset();
        }
        SetLang(0); Reset();

        // ---- 3) English: nothing changed for the real keys (FEH_LOA is the Game tile, FEC_RED the Controls tile)
        CHECK(Resolve("FEH_LOA") == "Game" && Resolve("FEC_RED") == "Controls");
        Tap(L.cx, rootY(5)); CHECK(g_kind == SK_OPTIONS && g_optRows[0].title == "Game");
        optItems[3].key = "FEC_RED"; g_optCache.top = NULL; g_optRowsGen = 0xFFFFFFFFu; Idle(3);
        CHECK(g_optRows[3].title == "Controls"); ComputeLayout(); Tap(L.pOptCx, PageRowCY(3)); CHECK(g_pageBg == 2);           // the Controls picture (it used to depend on a key the game does not have)
        optItems[3].key = "Mods settings"; g_optCache.top = NULL; g_optRowsGen = 0xFFFFFFFFu; Reset();
        SetLang(1); Idle(3); Tap(L.cx, rootY(5)); ComputeLayout(); Tap(L.pOptCx, PageRowCY(0)); CHECK(g_kind == SK_PAGE && g_pageTitle == "Jeu" && g_pageBg == 4);   // (French game: the key picks the picture, not the English title)
        SetLang(0); Reset();

        // ---- 4) Indonesian wording requested on the device (1.1.8)
        g_cfgLang = LG_ID; Idle(3);
        CHECK(Tr("RESUME") == "LANJUTKAN" && Tr("START GAME") == "MULAI PERMAINAN" && Tr("START NEW GAME") == "MULAI PERMAINAN BARU" && Tr("OPTIONS") == "PENGATURAN" && Tr("SETTINGS") == "PENGATURAN"
              && Tr("QUIT GAME") == "KELUAR PERMAINAN" && Tr("GAME") == "PERMAINAN" && Tr("NEW GAME") == "PERMAINAN BARU" && Tr("LOAD GAME") == "MUAT PERMAINAN" && Tr("SAVE GAME") == "SIMPAN PERMAINAN" && Tr("DELETE GAME") == "HAPUS PERMAINAN");
        CHECK(TitleOf("GAME") == "Permainan" && TitleOf("QUIT GAME") == "Keluar Permainan" && OptionsTitle() == "Pengaturan");
        CHECK(Resolve("FEH_LOA") == "PERMAINAN" && Resolve("FEC_RED") == "KONTROL" && Resolve("FEO_LAN") == "BAHASA");                // the Options tiles
        CHECK(Resolve("FEL_ENG") == "INGGRIS" && Resolve("FEL_FRE") == "PRANCIS" && Resolve("FEL_RUS") == "RUSIA");
        Reset(); CHECK(Labels() == "LANJUTKAN|MULAI PERMAINAN BARU|PETA|STATISTIK|RINGKASAN|PENGATURAN|KELUAR PERMAINAN");
        Tap(L.cx, rootY(5)); CHECK(g_kind == SK_OPTIONS && g_optRows[0].title == "Permainan" && g_optRows[0].label == "PERMAINAN");
        { bool anyGame = false; const char* all[] = { "ARE YOU SURE YOU WANT TO QUIT THE GAME", "ARE YOU SURE YOU WANT TO LOAD THIS GAME", "GAME SAVED", "NOT AVAILABLE WITHOUT A RUNNING GAME" };
          for(const char* a : all) { std::string t = PhraseFor(LG_ID, a); if(Tr(a) != a) t = Tr(a); if(t.find("GAME") != std::string::npos) anyGame = true; } CHECK(!anyGame); }          // no half-English "GAME" left in the tables
        { bool bad = false; for(int i = 0; i < kTxCount; ++i) { std::string t = kTx[i].id; if(t.find("GAME") != std::string::npos && strcmp(kTx[i].en, "GAME") != 0) bad = true; } CHECK(!bad); }
        Tap(L.pBackL + 30, L.pBackT + 20); Idle(40);
        // the mod phrases in Indonesian
        CHECK(PhraseFor(LG_ID, "Enable Earshot") == "AKTIFKAN EARSHOT" && PhraseFor(LG_ID, "Footsteps volume") == "VOLUME LANGKAH KAKI" && PhraseFor(LG_ID, "Hitmarker volume") == "VOLUME PENANDA HIT"
              && PhraseFor(LG_ID, "Max NPC footsteps") == "MAKS LANGKAH KAKI NPC" && PhraseFor(LG_ID, "Sound volume") == "VOLUME SUARA" && PhraseFor(LG_ID, "Show speedometer") == "TAMPILKAN SPEEDOMETER");
        CHECK(PhraseFor(LG_ID, "Frobnicate the widget").empty() && PhraseFor(LG_ID, "ENABLE").size() > 0 && PhraseFor(LG_ID, "Enable ").size() > 0 && PhraseFor(LG_ID, "VOLUME") == "VOLUME");

        // ---- 5) Arabic: the labels from the Earshot page (device screenshot) and the language names
        g_cfgLang = LG_AR; SetLang(0); Reset(); CHECK(UiLang() == LG_AR && IsRTL());
        CHECK(PhraseFor(LG_AR, "Enable Earshot") == "\xD8\xAA\xD9\x81\xD8\xB9\xD9\x8A\xD9\x84 EARSHOT");                                  // "<enable> EARSHOT": the mod's name stays Latin
        CHECK(PhraseFor(LG_AR, "Replace original sounds") == "استبدال الأصوات الأصلية" && PhraseFor(LG_AR, "Ricochet") == "ارتداد الرصاص" && PhraseFor(LG_AR, "Distant shots from") == "طلقات بعيدة من");
        CHECK(PhraseFor(LG_AR, "Hitmarkers") == "علامات الإصابة" && PhraseFor(LG_AR, "Hitmarker volume") == "مستوى صوت علامة الإصابة" && PhraseFor(LG_AR, "Footsteps") == "الخطوات" && PhraseFor(LG_AR, "Footsteps volume") == "مستوى صوت الخطوات");
        CHECK(PhraseFor(LG_AR, "NPC footsteps") == "خطوات الشخصيات" && PhraseFor(LG_AR, "Max NPC footsteps") == "الحد الأقصى لخطوات الشخصيات" && PhraseFor(LG_AR, "Master volume") == "مستوى الصوت الرئيسي");
        CHECK(Resolve("FEL_ENG") == "الإنجليزية" && Resolve("FEL_FRE") == "الفرنسية" && Resolve("FEL_RUS") == "الروسية" && Resolve("FEL_GER") == "الألمانية" && Resolve("FEL_ITA") == "الإيطالية" && Resolve("FEL_SPA") == "الإسبانية" && Resolve("FEL_JPN") == "اليابانية");
        CHECK(Resolve("FEH_LOA") == "اللعبة" && Resolve("FEC_RED") == "التحكم");
        // all of the Earshot page translated, on screen: labels at the right, toggles in Arabic, nothing left in English except the mod's name
        { void* keep0 = dispItems[0]; void* keep1 = dispItems[1]; void* keep2 = dispItems[2]; void* keep3 = dispItems[3]; void* keep4 = dispItems[4];
          const char* names[] = { "Enable Earshot", "Replace original sounds", "Ricochet", "Distant shots from", "Hitmarkers", "Hitmarker volume", "Footsteps", "Footsteps volume", "NPC footsteps", "Max NPC footsteps" };
          static void* ear[12]; for(int i = 0; i < 10; ++i) ear[i] = MkT(MkSetting(names[i], i % 2 ? 12 : 3)); SetupPage(pGame, ear, 10);
          Tap(L.cx, rootY(5)); ComputeLayout(); Tap(L.pOptCx, PageRowCY(0)); g_pageRowsTop = NULL; Idle(3);
          CHECK(g_kind == SK_PAGE && g_pageRows.size() == 10);
          { bool allAr = true; for(auto& r : g_pageRows) if(!HasArabic(r.label)) allAr = false; CHECK(allAr); }
          CHECK(g_pageRows[0].label == "تفعيل EARSHOT" && g_pageRows[9].label == "الحد الأقصى لخطوات الشخصيات");
          Shot("24_ar_earshot");
          { opsBuf.clear(); Frame(false, 0, 0); auto o = ParseOps(opsBuf); float lo, hi;
            CHECK(Ink(o, 5, PageRowCY(2), PageStep() * 0.45f, &lo, &hi) && fabsf(hi - (L.W - L.pLabelX)) < 0.012f * L.W); }                                  // labels right-aligned (the PC mirror layout)
          SetupPage(pGame, gameItems, 2);                                                                   // (the fixture's own two items are untouched: the Earshot page used its own array)
          dispItems[0] = keep0; dispItems[1] = keep1; dispItems[2] = keep2; dispItems[3] = keep3; dispItems[4] = keep4; g_pageRowsTop = NULL; }
        Reset();

        // ---- 6) Arabic is bigger (BACK and the rows looked small and thin next to the PC letters)
        { const Glyph& al = g_bank.ar[ArSlot(0xFE8D)]; const Glyph& tl = g_beck.ar[ArSlot(0xFE8D)];
          CHECK(al.valid && (float)al.h >= 1.35f * (float)g_bank.capH && (float)al.h <= 1.60f * (float)g_bank.capH);                                      // bank: alef 1.45 x the cap height of the PC font (was 1.12)
          CHECK(tl.valid && (float)tl.h >= 1.05f * (float)g_beck.capH && (float)tl.h <= 1.2f * (float)g_beck.capH); }                                          // titles: 1.10 x (was 0.95)
        // BACK: Arabic and English end at the same right edge and sit on the same baseline
        { Tap(L.cx, rootY(5)); CHECK(g_kind == SK_OPTIONS); float loA, hiA, loE, hiE; float by = 0.93f * L.H;
          opsBuf.clear(); Frame(false, 0, 0); auto oa = ParseOps(opsBuf); bool a = Ink(oa, 5, by, 0.04f * L.H, &loA, &hiA);
          g_cfgLang = 0; Idle(3); opsBuf.clear(); Frame(false, 0, 0); auto oe = ParseOps(opsBuf); bool e = Ink(oe, 1, by, 0.04f * L.H, &loE, &hiE);
          CHECK(a && e && fabsf(hiA - hiE) < 0.012f * L.W && (hiA - loA) > 0.03f * L.W);
          g_cfgLang = LG_AR; Idle(3); }
        Reset();

        // ---- 7) ArabicMirror=0 keeps the PC layout (labels left, values right); the letters are still Arabic
        { std::string keepDir = g_dirPC; mkdir("/tmp/pcm_lang", 0777); g_dirPC = "/tmp/pcm_lang/";
          FILE* f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs("ArabicMirror=0\nLanguage=Arabic\n", f); fclose(f); LoadConfig(); CHECK(!g_cfgArMirror && g_cfgLang == LG_AR && UiLang() == LG_AR && !IsRTL());
          Tap(L.cx, rootY(5)); ComputeLayout(); Tap(L.pOptCx, PageRowCY(1)); g_pageRowsTop = NULL; Idle(3); CHECK(g_kind == SK_PAGE);
          { opsBuf.clear(); Frame(false, 0, 0); auto o = ParseOps(opsBuf); float lo, hi; CHECK(Ink(o, 5, PageRowCY(0), PageStep() * 0.45f, &lo, &hi) && fabsf(lo - L.pLabelX) < 0.012f * L.W); }
          f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs("ArabicMirror=1\n", f); fclose(f); LoadConfig(); CHECK(g_cfgArMirror && IsRTL());
          f = fopen("/tmp/pcm_lang/pcmenu.ini", "w"); fputs("Language=Arabic\n", f); fclose(f); g_cfgArMirror = false; LoadConfig(); CHECK(!g_cfgArMirror);       // (a missing key leaves the value alone; the default is set at the declaration)
          g_cfgArMirror = true; g_dirPC = keepDir; }
        g_cfgLang = 0; SetLang(0); Reset();
        CHECK(Labels() == "RESUME|START NEW GAME|MAP|STATS|BRIEF|OPTIONS|QUIT GAME");
        g_pageRowsTop = NULL; for(void* m : r10Mem) free(m);
    }
    // =================================================================================== R11: v1.1.9 - INDONESIAN.GXT (game texts / subtitles)
    printf("R11 v1.1.9: GXT reader, CText::Get hook, Indonesian subtitles, menu texts from the file\n");
    {
        struct GKV { const char* k; const char* v; };
        struct GTab { const char* name; std::vector<GKV> kv; };
        auto P32 = [](std::vector<uint8_t>& b, uint32_t v) { for(int i = 0; i < 4; ++i) b.push_back((uint8_t)(v >> (8 * i))); };
        auto MakeGxt = [&](int bits, bool header, const std::vector<GTab>& tabs, bool sortKeys = true)
        {
            std::vector<std::vector<uint8_t>> bodies;
            for(size_t ti = 0; ti < tabs.size(); ++ti)
            {
                std::vector<uint8_t> dat; std::vector<std::pair<uint32_t, uint32_t>> ent;      // (hash, byte offset)
                for(const GKV& kv : tabs[ti].kv)
                {
                    ent.push_back({ gxt::KeyHash(kv.k), (uint32_t)dat.size() });
                    for(const unsigned char* c = (const unsigned char*)kv.v; ; ++c) { dat.push_back(*c); if(bits == 16) dat.push_back(0); if(!*c) break; }
                }
                if(sortKeys) std::sort(ent.begin(), ent.end());
                std::vector<uint8_t> b;
                if(ti > 0) { char nm[8] = {0}; strncpy(nm, tabs[ti].name, 8); b.insert(b.end(), nm, nm + 8); }
                b.insert(b.end(), { 'T', 'K', 'E', 'Y' }); P32(b, (uint32_t)(ent.size() * 8));
                for(auto& e : ent) { P32(b, e.second); P32(b, e.first); }
                b.insert(b.end(), { 'T', 'D', 'A', 'T' }); P32(b, (uint32_t)dat.size()); b.insert(b.end(), dat.begin(), dat.end());
                bodies.push_back(b);
            }
            std::vector<uint8_t> f;
            if(header) { f.push_back(4); f.push_back(0); f.push_back((uint8_t)bits); f.push_back(0); }
            f.insert(f.end(), { 'T', 'A', 'B', 'L' }); P32(f, (uint32_t)(12 * tabs.size()));
            uint32_t off = (uint32_t)f.size() + 12 * (uint32_t)tabs.size();
            for(size_t ti = 0; ti < tabs.size(); ++ti) { char nm[8] = {0}; strncpy(nm, tabs[ti].name, 8); f.insert(f.end(), nm, nm + 8); P32(f, off); off += (uint32_t)bodies[ti].size(); }
            for(auto& b : bodies) f.insert(f.end(), b.begin(), b.end());
            return f;
        };
        auto W16 = [](const char* s) { std::vector<uint16_t> v; for(; *s; ++s) v.push_back((unsigned char)*s); v.push_back(0); return v; };
        auto Same = [&](const uint16_t* w, const char* s) { if(!w) return false; std::vector<uint16_t> e = W16(s); for(size_t i = 0; i < e.size(); ++i) if(w[i] != e[i]) return false; return true; };
        auto Str = [](const unsigned short* w) { std::string s; for(int i = 0; w && w[i] && i < 200; ++i) s += (char)(w[i] < 128 ? w[i] : '?'); return s; };
        auto Write = [](const std::string& path, const std::vector<uint8_t>& b) { FILE* f = fopen(path.c_str(), "wb"); if(f) { fwrite(b.data(), 1, b.size(), f); fclose(f); } };
        const std::vector<GTab> good = {
            { "MAIN",   { { "FEO_AUD", "Suara" }, { "SUBT_1", "Ayo pergi ke Grove Street, CJ." }, { "FEX_UNK", "Frobnikasi" }, { "FEH_LOA", "Opsi Permainan" }, { "BOTH", "utama" }, { "EMPTY1", "" }, { "tutor", "Sentuh sudut kiri bawah layar." } } },
            { "SWEET1", { { "SW1_A", "Teks misi Sweet" }, { "BOTH", "misi" }, { "SW1_B", "Kembali ke ~r~rumah~w~." } } },
            { "INTRO",  { { "SW1_A", "Teks pembuka" } } } };

        // ---- 1) the hash is the game's CKeyGen::GetUppercaseKey (the value was computed from the game's own key table)
        CHECK(gxt::KeyHash("FEH_LOA") == 0xee8aa109u && gxt::KeyHash("feh_loa") == 0xee8aa109u && gxt::KeyHash("") == 0xFFFFFFFFu);

        // ---- 2) reader: 16 bit (the Android format) and 8 bit (PC files), with and without header, keys in any order
        for(int variant = 0; variant < 5; ++variant)
        {
            int bits = (variant == 1) ? 8 : 16; bool hdr = variant != 2; bool sorted = variant != 3;
            std::vector<uint8_t> f = MakeGxt(bits, hdr, good, sorted);
            if(variant == 4) f[2] = 0x37;                                                    // a header with a nonsense char width: the game ignores it, so does the reader (16 bit)
            gxt::Db db; std::string err; bool ok = db.Parse(f.data(), f.size(), &err);
            CHECK(ok && db.tabs.size() == 3 && db.keyCount == 11 && db.badTables == 0 && db.badKeys == 0);
            CHECK(Same(db.Find("FEO_AUD"), "Suara") && Same(db.Find("feo_aud"), "Suara") && Same(db.Find("SUBT_1"), "Ayo pergi ke Grove Street, CJ."));
            CHECK(Same(db.Find("tutor"), "Sentuh sudut kiri bawah layar.") && Same(db.Find("TUTOR"), "Sentuh sudut kiri bawah layar."));      // the game upper-cases keys
            CHECK(db.Find("SW1_A") == NULL && db.Find("SW1_A", "") == NULL && db.Find("SW1_A", "NOPE") == NULL);                               // a mission text needs its mission table
            CHECK(Same(db.Find("SW1_A", "SWEET1"), "Teks misi Sweet") && Same(db.Find("SW1_A", "INTRO"), "Teks pembuka") && db.Find("SW1_A", "SWEET") == NULL && db.Find("SW1_A", "SWEET12") == NULL);
            CHECK(Same(db.Find("BOTH", "SWEET1"), "utama") && Same(db.Find("BOTH"), "utama"));                                                // MAIN is searched first, like the game
            CHECK(Same(db.Find("SW1_B", "SWEET1"), "Kembali ke ~r~rumah~w~.") && db.Find("NOT_THERE") == NULL && db.Find(NULL) == NULL && db.Find("") == NULL);
            CHECK(db.Find("EMPTY1") != NULL && db.Find("EMPTY1")[0] == 0);
            CHECK(Same(db.FindAnywhere("SW1_B"), "Kembali ke ~r~rumah~w~."));
        }

        // ---- 3) damaged files never crash and are refused / partly used
        { gxt::Db d; std::string e;
          CHECK(!d.Parse((const uint8_t*)"", 0, &e) && !d.Parse((const uint8_t*)"TABL", 4, &e));
          std::vector<uint8_t> junk(500); for(size_t i = 0; i < junk.size(); ++i) junk[i] = (uint8_t)(i * 37 + 11); CHECK(!d.Parse(junk.data(), junk.size(), &e) && !e.empty());
          std::vector<uint8_t> f = MakeGxt(16, true, good);
          CHECK(!d.Parse(f.data(), 30, &e));                                                // cut inside the tables
          for(size_t cut = 0; cut < f.size(); cut += 7) { gxt::Db x; x.Parse(f.data(), cut, NULL); }          // every truncation: no crash
          std::vector<uint8_t> g = f; g[4 + 4] = 0xFF; g[4 + 5] = 0xFF; g[4 + 6] = 0xFF; g[4 + 7] = 0x7F; CHECK(!d.Parse(g.data(), g.size(), &e));   // TABL size far past the end
          // a mission table whose offset points outside: skipped, the rest is used
          g = f; size_t ent = 4 + 8 + 12 * 1 + 8; g[ent] = 0xF0; g[ent + 1] = 0xFF; g[ent + 2] = 0xFF; g[ent + 3] = 0x0F;      // entry 1 (SWEET1): offset
          CHECK(d.Parse(g.data(), g.size(), &e) && d.badTables == 1 && d.tabs.size() == 2 && Same(d.Find("FEO_AUD"), "Suara") && d.Find("SW1_A", "SWEET1") == NULL && Same(d.Find("SW1_A", "INTRO"), "Teks pembuka"));
          // a MAIN table with no texts is not a language file
          std::vector<GTab> emptyMain = { { "MAIN", {} } }; std::vector<uint8_t> em = MakeGxt(16, true, emptyMain); CHECK(!d.Parse(em.data(), em.size(), &e));
          // odd byte offsets in a 16 bit file are skipped (the game would read garbage there)
          std::vector<uint8_t> od = MakeGxt(16, true, { { "MAIN", { { "AA", "x" }, { "BB", "yy" } } } }); size_t tk = 4 + 8 + 12 + 8; od[tk] = 1; od[tk + 1] = 0;      // first sorted entry's offset made odd
          gxt::Db o2; bool okOdd = o2.Parse(od.data(), od.size(), NULL); CHECK(okOdd ? (o2.badKeys == 1 && o2.keyCount == 1) : true);
          // duplicates: the first one in the file wins
          std::vector<uint8_t> dup = MakeGxt(16, true, { { "MAIN", { { "DUP", "first" }, { "DUP", "second" }, { "OTHER", "o" } } } });
          gxt::Db d3; CHECK(d3.Parse(dup.data(), dup.size(), NULL) && d3.keyCount == 2 && d3.Find("DUP") != NULL && d3.Find("OTHER") != NULL);
        }
        // ---- 4) random damage (the sanitizers watch every read)
        { std::vector<uint8_t> f = MakeGxt(16, true, good); unsigned seed = 12345; auto Rnd = [&]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
          int okParses = 0;
          for(int it = 0; it < 4000; ++it)
          {
              std::vector<uint8_t> g = f; int n = 1 + Rnd() % 8;
              for(int k = 0; k < n; ++k) g[Rnd() % g.size()] = (uint8_t)Rnd();
              if(Rnd() % 4 == 0) g.resize(Rnd() % g.size());
              gxt::Db d; if(d.Parse(g.data(), g.size(), NULL)) { ++okParses; for(const char* k : { "FEO_AUD", "SUBT_1", "BOTH", "SW1_A", "tutor", "zzz" }) { const uint16_t* t = d.Find(k, "SWEET1"); if(t) { size_t l = 0; while(t[l] && l < 100000) ++l; (void)l; } d.FindAnywhere(k); } }
          }
          CHECK(okParses > 100); }

        // ---- 5) which file is used
        { auto Mk = [&](const std::string& dir, const char* name) { std::string cmd = "mkdir -p '" + dir + "'"; (void)!system(cmd.c_str()); if(name) Write(dir + name, MakeGxt(16, true, good)); };
          std::string keepDir = g_dirPC;
          (void)!system("rm -rf /tmp/pcm_gx1 /tmp/pcm_gx2 /tmp/pcm_gx3 /tmp/pcm_gx4 /tmp/pcm_gx5");
          Mk("/tmp/pcm_gx1/files/PCMenu/", "Indonesian.GXT"); g_dirPC = "/tmp/pcm_gx1/files/PCMenu/"; CHECK(FindIdGxtFile() == "/tmp/pcm_gx1/files/PCMenu/Indonesian.GXT");
          Mk("/tmp/pcm_gx2/files/PCMenu/", "whatever.gxt"); g_dirPC = "/tmp/pcm_gx2/files/PCMenu/"; CHECK(FindIdGxtFile() == "/tmp/pcm_gx2/files/PCMenu/whatever.gxt");     // the only .gxt
          Mk("/tmp/pcm_gx3/files/PCMenu/", "a.gxt"); Mk("/tmp/pcm_gx3/files/PCMenu/", "b.gxt"); g_dirPC = "/tmp/pcm_gx3/files/PCMenu/"; CHECK(FindIdGxtFile().empty());           // two unknown files: no guess
          Mk("/tmp/pcm_gx3/files/PCMenu/", "ID.GXT"); CHECK(FindIdGxtFile() == "/tmp/pcm_gx3/files/PCMenu/ID.GXT");
          Mk("/tmp/pcm_gx4/files/PCMenu/", NULL); Write("/tmp/pcm_gx4/files/INDONESIAN.GXT", MakeGxt(16, true, good)); g_dirPC = "/tmp/pcm_gx4/files/PCMenu/"; CHECK(FindIdGxtFile() == "/tmp/pcm_gx4/files/INDONESIAN.GXT");    // next to the PCMenu folder
          Mk("/tmp/pcm_gx5/files/PCMenu/", NULL); Write("/tmp/pcm_gx5/files/american.gxt", MakeGxt(16, true, good)); g_dirPC = "/tmp/pcm_gx5/files/PCMenu/"; CHECK(FindIdGxtFile().empty());      // a parent-folder file needs the right name
          g_dirPC = keepDir; }

        // ---- 6) the hook: CText::Get answers from the file only while Indonesian is the PC menu language
        static char fakeMission[16]; fakeMission[0] = 0;
        fnMissionName = [](void*, char* out) { strcpy(out, fakeMission); };
        TextGet_orig = fText; gxtMissing[0] = 0;                                      // (the real game's "no such key" text is an empty string)
        auto ResetId = [&]() { g_idState = 0; if(g_idDb) { delete g_idDb; g_idDb = NULL; } g_idFile.clear(); };
        std::string keepDir = g_dirPC; (void)!system("rm -rf /tmp/pcm_gxh; mkdir -p /tmp/pcm_gxh"); g_dirPC = "/tmp/pcm_gxh/";
        Write("/tmp/pcm_gxh/INDONESIAN.GXT", MakeGxt(16, true, good));
        gameLang = 0; fakeFront[OFF_FE_LANGUAGE] = 0; g_cfgIdGxt = true; g_cfgIdGxtLog = true; ResetId(); remove("/tmp/pcm_gxh/idgxt_missing.txt");
        g_cfgLang = 0;
        CHECK(Str(HookOf_CText_Get(NULL, "FEO_AUD")) == "Audio" && g_idState.load() == 0);                  // not Indonesian: the game's text, the file is not even opened
        g_cfgLang = LG_ID;
        CHECK(Str(HookOf_CText_Get(NULL, "FEO_AUD")) == "Suara" && g_idState.load() == 2);                  // a text the game has, in Indonesian
        CHECK(Str(HookOf_CText_Get(NULL, "SUBT_1")) == "Ayo pergi ke Grove Street, CJ.");                   // a text only the file has
        CHECK(Str(HookOf_CText_Get(NULL, "feo_aud")) == "Suara");
        CHECK(Str(HookOf_CText_Get(NULL, "FEO_DIS")) == "Display");                                         // not in the file: the game's text
        CHECK(HookOf_CText_Get(NULL, "NOPE_X") == gxtMissing);                                              // neither: the game's "missing" answer, untouched
        CHECK(HookOf_CText_Get(NULL, "") == gxtMissing);                                                    // an empty key: the game's answer
        CHECK(HookOf_CText_Get(NULL, "EMPTY1") == gxtMissing);                                              // an empty text in the file counts as missing
        // mission tables
        fakeMission[0] = 0;        CHECK(HookOf_CText_Get((void*)0x77, "SW1_A") == gxtMissing);             // no mission text loaded
        strcpy(fakeMission, "SWEET1"); CHECK(Str(HookOf_CText_Get((void*)0x77, "SW1_A")) == "Teks misi Sweet" && Str(HookOf_CText_Get((void*)0x77, "SW1_B")) == "Kembali ke ~r~rumah~w~.");
        CHECK(Str(HookOf_CText_Get((void*)0x77, "BOTH")) == "utama");                                       // MAIN first
        strcpy(fakeMission, "INTRO");  CHECK(Str(HookOf_CText_Get((void*)0x77, "SW1_A")) == "Teks pembuka" && HookOf_CText_Get((void*)0x77, "SW1_B") == gxtMissing);
        strcpy(fakeMission, "OTHER1"); CHECK(HookOf_CText_Get((void*)0x77, "SW1_A") == gxtMissing);
        fakeMission[0] = 0;
        // the pointer the game gets stays valid and identical (the game compares / keeps text pointers)
        { const unsigned short* a = HookOf_CText_Get(NULL, "SUBT_1"); const unsigned short* b = HookOf_CText_Get(NULL, "SUBT_1"); CHECK(a == b && Str(a) == "Ayo pergi ke Grove Street, CJ."); }
        // the log of what the file lacks: the game's text exists, once per key
        HookOf_CText_Get(NULL, "FEO_DIS"); HookOf_CText_Get(NULL, "FEO_DIS"); HookOf_CText_Get(NULL, "FEP_RES");
        { std::string log = ReadFileText("/tmp/pcm_gxh/idgxt_missing.txt");
          CHECK(log.find("FEO_DIS\tMAIN\tDisplay\n") != std::string::npos && log.find("FEP_RES\tMAIN\tResume\n") != std::string::npos);
          size_t first = log.find("FEO_DIS"), second = log.find("FEO_DIS", first + 1); CHECK(first != std::string::npos && second == std::string::npos);
          CHECK(log.find("NOPE_X") == std::string::npos && log.find("FEO_AUD") == std::string::npos); }          // keys nobody has / keys the file has are not listed
        g_cfgIdGxtLog = false; remove("/tmp/pcm_gxh/idgxt_missing.txt"); HookOf_CText_Get(NULL, "FEH_STA"); CHECK(ReadFileText("/tmp/pcm_gxh/idgxt_missing.txt").empty()); g_cfgIdGxtLog = true;
        g_cfgIdGxt = false; CHECK(Str(HookOf_CText_Get(NULL, "FEO_AUD")) == "Audio"); g_cfgIdGxt = true;     // IndonesianText=0
        g_cfgLang = LG_AR; CHECK(Str(HookOf_CText_Get(NULL, "FEO_AUD")) == "Audio");                         // Arabic: the game's texts (no Arabic file)
        g_cfgLang = LG_ID; CHECK(Str(HookOf_CText_Get(NULL, "FEO_AUD")) == "Suara");
        // no file / a file that is not usable: the game's text, and the file is not searched again
        ResetId(); remove("/tmp/pcm_gxh/INDONESIAN.GXT"); CHECK(Str(HookOf_CText_Get(NULL, "FEO_AUD")) == "Audio" && g_idState.load() == 3);
        Write("/tmp/pcm_gxh/INDONESIAN.GXT", MakeGxt(16, true, good)); CHECK(Str(HookOf_CText_Get(NULL, "FEO_AUD")) == "Audio");              // (decided once per run)
        ResetId(); { std::vector<uint8_t> bad(900); for(size_t i = 0; i < bad.size(); ++i) bad[i] = (uint8_t)(i * 7); Write("/tmp/pcm_gxh/INDONESIAN.GXT", bad); }
        CHECK(Str(HookOf_CText_Get(NULL, "FEO_AUD")) == "Audio" && g_idState.load() == 3);
        // 8 bit PC file through the same hook
        ResetId(); Write("/tmp/pcm_gxh/INDONESIAN.GXT", MakeGxt(8, true, good)); CHECK(Str(HookOf_CText_Get(NULL, "FEO_AUD")) == "Suara" && g_idState.load() == 2);

        // ---- 7) the PC menu's own texts: the tables keep priority, the file fills the rest
        ResetId(); Write("/tmp/pcm_gxh/INDONESIAN.GXT", MakeGxt(16, true, good)); gameLang = 0; fakeFront[OFF_FE_LANGUAGE] = 0; g_cfgLang = LG_ID;
        { bool dr = false;
          CHECK(Resolve("FEH_LOA", true, &dr) == "PERMAINAN" && dr);                                       // curated wording wins over the file's "Opsi Permainan"
          CHECK(Resolve("FEX_UNK", true, &dr) == "Frobnikasi" && dr);                                      // no table wording: the file's text
          CHECK(Resolve("FEX_UNK", false, &dr) == "Frobnicate");                                           // localize = false: the game's own text
          CHECK(Resolve("SOME PLAIN MOD TEXT", true, &dr) == "SOME PLAIN MOD TEXT" && dr);                 // a mod's own text is not a key
          gameLang = 5; fakeFront[OFF_FE_LANGUAGE] = 5;                                                    // Russian game: the font cannot draw its text
          CHECK(Resolve("FEX_UNK", true, &dr) == "Frobnikasi" && dr);                                      // ... the Indonesian file can
          CHECK(Resolve("FEH_LOA", true, &dr) == "PERMAINAN" && dr);
          CHECK(Resolve("FED_BRI", true, &dr).find('?') == std::string::npos);
          g_cfgLang = 0; CHECK(Resolve("FEX_UNK", true, &dr).empty() && !dr);                              // not Indonesian: as in 1.1.8 (the caller labels it "OPTION n")
          g_cfgLang = LG_ID;
          ResetId(); remove("/tmp/pcm_gxh/INDONESIAN.GXT");                                                // no file: as in 1.1.8
          CHECK(Resolve("FEX_UNK", true, &dr).empty() && !dr); CHECK(Resolve("FEH_LOA", true, &dr) == "PERMAINAN");
          gameLang = 6; fakeFront[OFF_FE_LANGUAGE] = 6; Write("/tmp/pcm_gxh/INDONESIAN.GXT", MakeGxt(16, true, good)); ResetId();
          CHECK(Resolve("FEX_UNK", true, &dr) == "Frobnikasi" && dr);                                      // Japanese game too
          gameLang = 1; fakeFront[OFF_FE_LANGUAGE] = 1;                                                    // French game: drawable text, no table wording for "Frobnicate"
          CHECK(Resolve("FEX_UNK", true, &dr).find('?') == std::string::npos);
          gameLang = 0; fakeFront[OFF_FE_LANGUAGE] = 0; }
        // a file text the font cannot draw is not used (never '?')
        { std::vector<GTab> odd = { { "MAIN", { { "FEX_UNK", "Caf\xE9 \xC0\xC1" } } } }; std::vector<uint8_t> f = MakeGxt(16, true, odd); ResetId(); Write("/tmp/pcm_gxh/INDONESIAN.GXT", f);
          gameLang = 5; fakeFront[OFF_FE_LANGUAGE] = 5; bool dr = true; std::string s = Resolve("FEX_UNK", true, &dr); CHECK(s.find('?') == std::string::npos);
          gameLang = 0; fakeFront[OFF_FE_LANGUAGE] = 0; }
        ResetId(); g_cfgLang = 0; gameLang = 0; fakeFront[OFF_FE_LANGUAGE] = 0; gxtMissing[0] = '?'; TextGet_orig = NULL; fnMissionName = NULL; g_dirPC = keepDir;
    }
#endif

    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
const SLInterfaceID SL_IID_ENGINE = 0, SL_IID_PLAY = 0, SL_IID_ANDROIDSIMPLEBUFFERQUEUE = 0;
SLresult slCreateEngine(SLObjectItf*, SLuint32, const void*, SLuint32, const SLInterfaceID*, const SLboolean*) { return 1; }
