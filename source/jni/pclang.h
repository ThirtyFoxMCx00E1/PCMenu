// PCMenu - text language layer (included by main.cpp, after Upper() / SafeCopy() / g_cfgIndonesian)
//
//  * The game's own language (FrontEndMenuManager+0x48: 0 English, 1 French, 2 German, 3 Italian, 4 Spanish, 5 Russian,
//    6 Japanese - read from CText::Load, which picks AMERICAN/FRENCH/GERMAN/... .GXT from that byte) decides which GXT the
//    stock texts come from. PC menu texts that come from a GXT key therefore follow the game language by themselves.
//  * The font atlases only hold ASCII. GXT accents are folded to the plain capital letter (the game's own French upper-case
//    table does the same). The code page was read from CText::GetUpperCase of libGTASA: 0x80..0x95 A-grave.. U-umlaut,
//    0x96 sharp s, 0x97..0xAC the same letters in lower case, 0xAD/0xAE N/n tilde, 0xAF/0xB0 inverted ? and !.
//    Text that cannot be drawn (Cyrillic, Japanese) falls back to the English name of the GXT key.
//  * "Indonesian" and "Arabic" are languages of the PC menu only: the game has no Indonesian or Arabic text file. Where a text comes
//    from the game GXT in English (or its key is known) it is translated with the tables below; anything unknown stays as the game
//    wrote it. Arabic is UTF-8 here and gets its letter forms / right-to-left order when drawn (arabic.h), from the glyph atlases
//    bank_ar / beck_ar; without those files the Arabic choice is simply not offered.
#ifndef PCLANG_H
#define PCLANG_H

enum { LG_EN = 0, LG_FR, LG_DE, LG_IT, LG_ES, LG_RU, LG_JA, LG_ID = 100, LG_AR = 101 };

static uint8_t* g_pFrontEnd = NULL;                 // FrontEndMenuManager (CMenuManager)
#define OFF_FE_LANGUAGE 0x48                        // same offset in the 32 and 64 bit builds (ldrsb [FrontEndMenuManager,#0x48] in CText::Load)

static int GameLanguage()                           // 0..6, or -1 when it cannot be read
{
    int8_t v = -1;
    if(!g_pFrontEnd || !SafeCopy(g_pFrontEnd + OFF_FE_LANGUAGE, &v, 1) || v < 0 || v > 6) return -1;
    return v;
}
static int UiLang()                                 // language of the PC menu's own texts
{
    if(g_cfgLang == LG_ID) return LG_ID;
    if(g_cfgLang == LG_AR && g_arOK) return LG_AR;                 // Arabic needs its glyph atlases
    int g = GameLanguage();
    return g < 0 ? (int)LG_EN : g;
}
static int LangStamp() { return UiLang() * 16 + (GameLanguage() + 1); }
static bool IsRTL() { return UiLang() == LG_AR && g_cfgArMirror; }   // the PC menu reads right to left (ArabicMirror=0 in pcmenu.ini keeps the left-to-right layout; the letters themselves are always right to left)

/* ---- GXT characters -> ASCII ---- */
static const char* const kFold[0x31] = {
    "A","A","A","A","AE","C","E","E","E","E","I","I","I","I","O","O","O","O","U","U","U","U",      // 0x80..0x95 (capitals)
    "ss",                                                                                            // 0x96
    "a","a","a","a","ae","c","e","e","e","e","i","i","i","i","o","o","o","o","u","u","u","u",      // 0x97..0xAC (small letters stay small: "Param\x9Etres" -> "Parametres")
    "N","n","","" };                                                                                 // 0xAD N-tilde, 0xAE n-tilde, 0xAF inverted ?, 0xB0 inverted !
/* appends the plain-ASCII form of one GXT character; false (and nothing appended) when the font cannot show it.
   The accent table above is the Latin code page of the English / French / German / Italian / Spanish GXT files. The Russian and the
   Japanese files use the same code points for Cyrillic / kana, so for them everything above 0x7F is "cannot be drawn" - folding it
   would print wrong Latin letters. Callers fall back to the English name of the text instead of showing '?' (v1.1.8). */
static bool FoldChar(unsigned c, std::string& out)
{
    if(c >= 32 && c < 127) { out += (char)c; return true; }
    if(c < 32) return true;                               // control characters: nothing to draw, nothing missing
    int g = GameLanguage();
    if(c >= 0x80 && c <= 0xB0 && g != LG_RU && g != LG_JA) { out += kFold[c - 0x80]; return true; }
    return false;
}
/* the readable part of a text that could not be drawn completely ("FPS (кадры)" -> "FPS"); empty when there is nothing worth showing */
static std::string DrawablePart(const std::string& s)
{
    std::string r; int alnum = 0; bool sp = false;
    for(size_t i = 0; i < s.size(); ++i)
    {
        unsigned char c = (unsigned char)s[i];
        bool an = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if(an) { if(sp && !r.empty()) r += ' '; sp = false; r += (char)c; ++alnum; }
        else sp = true;
    }
    return alnum >= 3 ? r : std::string();
}

/* ---- fixed PC menu phrases: English (canonical, upper case) -> fr / de / it / es / id ---- */
struct Tx { const char* en; const char* fr; const char* de; const char* it; const char* es; const char* id; const char* ar; };
static const Tx kTx[] = {
    { "MAIN MENU",      "MENU PRINCIPAL",       "HAUPTMENU",            "MENU PRINCIPALE",      "MENU PRINCIPAL",       "MENU UTAMA", "القائمة الرئيسية" },
    { "PAUSE MENU",     "MENU PAUSE",           "PAUSENMENU",           "MENU DI PAUSA",        "MENU DE PAUSA",        "MENU JEDA", "قائمة الإيقاف المؤقت" },
    { "RESUME",         "REPRENDRE",            "FORTSETZEN",           "RIPRENDI",             "CONTINUAR",            "LANJUTKAN", "متابعة" },
    { "START NEW GAME", "NOUVELLE PARTIE",      "NEUES SPIEL STARTEN",  "NUOVA PARTITA",        "NUEVA PARTIDA",        "MULAI PERMAINAN BARU", "بدء لعبة جديدة" },
    { "START GAME",     "COMMENCER LE JEU",     "SPIEL STARTEN",        "INIZIA IL GIOCO",      "INICIAR JUEGO",        "MULAI PERMAINAN", "بدء اللعبة" },
    { "MAP",            "CARTE",                "KARTE",                "MAPPA",                "MAPA",                 "PETA", "الخريطة" },
    { "STATS",          "STATISTIQUES",         "STATISTIKEN",          "STATISTICHE",          "ESTADISTICAS",         "STATISTIK", "الإحصائيات" },
    { "BRIEF",          "BRIEFING",             "BRIEFING",             "BRIEFING",             "RESUMEN",              "RINGKASAN", "الملخص" },
    { "OPTIONS",        "OPTIONS",              "OPTIONEN",             "OPZIONI",              "OPCIONES",             "PENGATURAN", "الخيارات" },
    { "MODS",           "MODS",                 "MODS",                 "MOD",                  "MODS",                 "MOD", "المودات" },
    { "QUIT GAME",      "QUITTER LE JEU",       "SPIEL BEENDEN",        "ESCI DAL GIOCO",       "SALIR DEL JUEGO",      "KELUAR PERMAINAN", "الخروج من اللعبة" },
    { "BACK",           "RETOUR",               "ZURUCK",               "INDIETRO",             "ATRAS",                "KEMBALI", "رجوع" },
    { "NEW GAME",       "NOUVELLE PARTIE",      "NEUES SPIEL",          "NUOVA PARTITA",        "NUEVA PARTIDA",        "PERMAINAN BARU", "لعبة جديدة" },
    { "LOAD GAME",      "CHARGER UNE PARTIE",   "SPIEL LADEN",          "CARICA PARTITA",       "CARGAR PARTIDA",       "MUAT PERMAINAN", "تحميل لعبة" },
    { "SAVE GAME",      "SAUVEGARDER",          "SPIEL SPEICHERN",      "SALVA PARTITA",        "GUARDAR PARTIDA",      "SIMPAN PERMAINAN", "حفظ اللعبة" },
    { "DELETE GAME",    "SUPPRIMER UNE PARTIE", "SPIEL LOSCHEN",        "ELIMINA PARTITA",      "BORRAR PARTIDA",       "HAPUS PERMAINAN", "حذف لعبة" },
    { "GAME",           "JEU",                  "SPIEL",                "GIOCO",                "JUEGO",                "PERMAINAN", "اللعبة" },
    { "CONTROLS",       "COMMANDES",            "STEUERUNG",            "CONTROLLI",            "CONTROLES",            "KONTROL", "التحكم" },
    { "AUDIO",          "AUDIO",                "AUDIO",                "AUDIO",                "AUDIO",                "AUDIO", "الصوت" },
    { "DISPLAY",        "AFFICHAGE",            "ANZEIGE",              "SCHERMO",              "PANTALLA",             "TAMPILAN", "العرض" },
    { "LANGUAGE",       "LANGUE",               "SPRACHE",              "LINGUA",               "IDIOMA",               "BAHASA", "اللغة" },
    { "MODS SETTINGS",  "PARAMETRES DES MODS",  "MOD-EINSTELLUNGEN",    "IMPOSTAZIONI MOD",     "AJUSTES DE MODS",      "PENGATURAN MOD", "إعدادات المودات" },
    { "SETTINGS",       "PARAMETRES",           "EINSTELLUNGEN",        "IMPOSTAZIONI",         "AJUSTES",              "PENGATURAN", "الإعدادات" },
    { "OPEN THE CONFIGURATION OF YOUR MODS", "OUVRIR LA CONFIGURATION DE VOS MODS", "KONFIGURATION DEINER MODS OFFNEN", "APRI LA CONFIGURAZIONE DEI TUOI MOD", "ABRIR LA CONFIGURACION DE TUS MODS", "BUKA PENGATURAN MOD ANDA", "افتح إعدادات المودات الخاصة بك" },
    { "PLEASE WAIT",    "PATIENTEZ",            "BITTE WARTEN",         "ATTENDERE",            "ESPERE",               "HARAP TUNGGU", "يرجى الانتظار" },
    { "NOTHING TO SHOW HERE", "RIEN A AFFICHER ICI", "HIER IST NICHTS ZU SEHEN", "NIENTE DA MOSTRARE", "NADA QUE MOSTRAR AQUI", "TIDAK ADA YANG DITAMPILKAN", "لا يوجد شيء لعرضه هنا" },
    { "NO MODS FOUND",  "AUCUN MOD TROUVE",     "KEINE MODS GEFUNDEN",  "NESSUN MOD TROVATO",   "NO SE ENCONTRARON MODS", "TIDAK ADA MOD DITEMUKAN", "لم يتم العثور على مودات" },
    { "MOD FOUND",      "MOD TROUVE",           "MOD GEFUNDEN",         "MOD TROVATO",          "MOD ENCONTRADO",       "MOD DITEMUKAN", "مود موجود" },
    { "MODS FOUND",     "MODS TROUVES",         "MODS GEFUNDEN",        "MOD TROVATI",          "MODS ENCONTRADOS",     "MOD DITEMUKAN", "مودات موجودة" },
    { "COULD NOT OPEN MOD SETTINGS", "IMPOSSIBLE D'OUVRIR LES PARAMETRES DES MODS", "MOD-EINSTELLUNGEN KONNTEN NICHT GEOFFNET WERDEN", "IMPOSSIBILE APRIRE LE IMPOSTAZIONI DEI MOD", "NO SE PUDIERON ABRIR LOS AJUSTES DE MODS", "TIDAK DAPAT MEMBUKA PENGATURAN MOD", "تعذر فتح إعدادات المود" },
    { "NO SETTINGS FOUND FOR", "AUCUN PARAMETRE POUR", "KEINE EINSTELLUNGEN FUR", "NESSUNA IMPOSTAZIONE PER", "NO HAY AJUSTES PARA", "TIDAK ADA PENGATURAN UNTUK", "لا توجد إعدادات للمود" },
    { "SHOWING ALL",    "AFFICHAGE DE TOUT",    "ALLE WERDEN ANGEZEIGT", "MOSTRO TUTTO",        "MOSTRANDO TODO",       "MENAMPILKAN SEMUA", "عرض الكل" },
    { "NOT AVAILABLE WITHOUT A RUNNING GAME", "INDISPONIBLE SANS PARTIE EN COURS", "OHNE LAUFENDES SPIEL NICHT VERFUGBAR", "NON DISPONIBILE SENZA UNA PARTITA IN CORSO", "NO DISPONIBLE SIN UNA PARTIDA EN CURSO", "TIDAK TERSEDIA TANPA PERMAINAN YANG BERJALAN", "غير متاح بدون لعبة قيد التشغيل" },
    { "ACTIVE",         "ACTIF",                "AKTIV",                "ATTIVO",               "ACTIVO",               "AKTIF", "نشط" },
    { "OPTION",         "OPTION",               "OPTION",               "OPZIONE",              "OPCION",               "OPSI", "خيار" },
    { "SLOT",           "EMPLACEMENT",          "SLOT",                 "SLOT",                 "RANURA",               "SLOT", "خانة" },
};
static const int kTxCount = (int)(sizeof(kTx) / sizeof(kTx[0]));

/* ---- Indonesian only: texts that come out of the game GXT (English wording) -> Indonesian ---- */
struct IdP { const char* en; const char* id; const char* ar; };
static const IdP kIdPhrases[] = {
    // values
    { "ON", "NYALA", "تشغيل" }, { "OFF", "MATI", "إيقاف" }, { "YES", "YA", "نعم" }, { "NO", "TIDAK", "لا" }, { "OK", "OK", "موافق" }, { "CANCEL", "BATAL", "إلغاء" },
    { "LOW", "RENDAH", "منخفض" }, { "MEDIUM", "SEDANG", "متوسط" }, { "HIGH", "TINGGI", "مرتفع" }, { "VERY HIGH", "SANGAT TINGGI", "مرتفع جدا" }, { "ULTRA", "ULTRA", "فائق" },
    { "NONE", "TIDAK ADA", "بدون" }, { "DEFAULT", "BAWAAN", "افتراضي" }, { "AUTO", "OTOMATIS", "تلقائي" }, { "ENABLED", "AKTIF", "مفعل" }, { "DISABLED", "NONAKTIF", "معطل" },
    { "STANDARD", "STANDAR", "قياسي" }, { "FAST", "CEPAT", "سريع" }, { "SLOW", "LAMBAT", "بطيء" }, { "LEFT", "KIRI", "يسار" }, { "RIGHT", "KANAN", "يمين" },
    // save slots
    { "EMPTY SLOT", "SLOT KOSONG", "خانة فارغة" }, { "CLOUD NOT AVAILABLE", "CLOUD TIDAK TERSEDIA", "السحابة غير متاحة" }, { "AUTOSAVE", "SIMPAN OTOMATIS", "حفظ تلقائي" },
    // display
    { "BRIGHTNESS", "KECERAHAN", "السطوع" }, { "RESOLUTION", "RESOLUSI", "الدقة" }, { "DRAW DISTANCE", "JARAK PANDANG", "مدى الرؤية" }, { "FRAME LIMITER", "PEMBATAS FPS", "محدد الإطارات" },
    { "FRAME RATE", "FRAME RATE", "معدل الإطارات" }, { "ANTI-ALIASING", "ANTI-ALIASING", "تنعيم الحواف" }, { "SHADOWS", "BAYANGAN", "الظلال" }, { "SHADOW QUALITY", "KUALITAS BAYANGAN", "جودة الظلال" },
    { "WATER QUALITY", "KUALITAS AIR", "جودة المياه" }, { "TEXTURE QUALITY", "KUALITAS TEKSTUR", "جودة الخامات" }, { "VISUAL FX QUALITY", "KUALITAS EFEK VISUAL", "جودة المؤثرات البصرية" },
    { "VISUAL EFFECTS", "EFEK VISUAL", "المؤثرات البصرية" }, { "WIDESCREEN", "LAYAR LEBAR", "شاشة عريضة" }, { "SHOW FPS", "TAMPILKAN FPS", "عرض معدل الإطارات" },
    // game
    { "SUBTITLES", "SUBTITEL", "الترجمة" }, { "SHOW SUBTITLES", "TAMPILKAN SUBTITEL", "عرض الترجمة" }, { "VIBRATION", "GETARAN", "الاهتزاز" }, { "HUD", "HUD", "الواجهة" }, { "SHOW HUD", "TAMPILKAN HUD", "عرض الواجهة" },
    { "RADAR", "RADAR", "الرادار" }, { "RADAR MODE", "MODE RADAR", "وضع الرادار" }, { "MAP AND BLIPS", "PETA DAN PENANDA", "الخريطة والعلامات" }, { "BLIPS ONLY", "HANYA PENANDA", "العلامات فقط" }, { "OFF RADAR", "RADAR MATI", "بدون رادار" },
    { "CLOUD SAVE", "SIMPAN CLOUD", "الحفظ السحابي" }, { "CAMERA", "KAMERA", "الكاميرا" }, { "VEHICLE CAMERA", "KAMERA KENDARAAN", "كاميرا المركبة" },
    // audio
    { "MASTER VOLUME", "VOLUME UTAMA", "مستوى الصوت الرئيسي" }, { "MUSIC VOLUME", "VOLUME MUSIK", "مستوى الموسيقى" }, { "SFX VOLUME", "VOLUME EFEK SUARA", "مستوى المؤثرات الصوتية" }, { "RADIO VOLUME", "VOLUME RADIO", "مستوى الراديو" },
    { "SOUND EFFECTS VOLUME", "VOLUME EFEK SUARA", "مستوى المؤثرات الصوتية" }, { "MUSIC", "MUSIK", "الموسيقى" }, { "SOUND EFFECTS", "EFEK SUARA", "المؤثرات الصوتية" }, { "SFX", "EFEK SUARA", "المؤثرات الصوتية" }, { "RADIO", "RADIO", "الراديو" },
    { "RADIO STATION", "STASIUN RADIO", "محطة الراديو" }, { "RADIO AUTO TUNE", "PENALAAN RADIO OTOMATIS", "الضبط التلقائي للراديو" }, { "RADIO AUTO-TUNE", "PENALAAN RADIO OTOMATIS", "الضبط التلقائي للراديو" },
    { "RADIO AUTOTUNE", "PENALAAN RADIO OTOMATIS", "الضبط التلقائي للراديو" }, { "STEREO", "STEREO", "ستيريو" }, { "MONO", "MONO", "أحادي" }, { "HRTF", "HRTF", "HRTF" },
    // controls
    { "CHANGE BUTTON POSITION", "UBAH POSISI TOMBOL", "تغيير موضع الأزرار" }, { "REDEFINE CONTROLS", "ATUR ULANG KONTROL", "إعادة تعيين التحكم" }, { "CONTROL SETUP", "PENGATURAN KONTROL", "إعداد التحكم" },
    { "CONTROLLER SETUP", "PENGATURAN KONTROLER", "إعداد وحدة التحكم" }, { "STEERING", "KEMUDI", "التوجيه" }, { "SENSITIVITY", "SENSITIVITAS", "الحساسية" }, { "CAMERA SENSITIVITY", "SENSITIVITAS KAMERA", "حساسية الكاميرا" },
    { "LOOK SENSITIVITY", "SENSITIVITAS PANDANGAN", "حساسية النظر" }, { "TOUCH SENSITIVITY", "SENSITIVITAS SENTUH", "حساسية اللمس" }, { "AUTO AIM", "BIDIK OTOMATIS", "التصويب التلقائي" }, { "GYRO", "GIROSKOP", "الجيروسكوب" },
    { "BUTTON OPACITY", "OPASITAS TOMBOL", "شفافية الأزرار" }, { "RESET TO DEFAULT", "KEMBALIKAN KE BAWAAN", "استعادة الإعدادات الافتراضية" }, { "RESET TO DEFAULTS", "KEMBALIKAN KE BAWAAN", "استعادة الإعدادات الافتراضية" }, { "RESTORE DEFAULTS", "KEMBALIKAN KE BAWAAN", "استعادة الإعدادات الافتراضية" },
    // dialogs
    { "ARE YOU SURE", "APAKAH ANDA YAKIN", "هل أنت متأكد" }, { "ARE YOU SURE YOU WANT TO QUIT", "APAKAH ANDA YAKIN INGIN KELUAR", "هل أنت متأكد أنك تريد الخروج" },
    { "ARE YOU SURE YOU WANT TO QUIT THE GAME", "APAKAH ANDA YAKIN INGIN KELUAR DARI PERMAINAN", "هل أنت متأكد أنك تريد الخروج من اللعبة" },
    { "ARE YOU SURE YOU WANT TO START A NEW GAME", "APAKAH ANDA YAKIN INGIN MEMULAI PERMAINAN BARU", "هل أنت متأكد أنك تريد بدء لعبة جديدة" },
    { "ARE YOU SURE YOU WANT TO LOAD THIS GAME", "APAKAH ANDA YAKIN INGIN MEMUAT PERMAINAN INI", "هل أنت متأكد أنك تريد تحميل هذه اللعبة" },
    { "ARE YOU SURE YOU WANT TO SAVE THIS GAME", "APAKAH ANDA YAKIN INGIN MENYIMPAN PERMAINAN INI", "هل أنت متأكد أنك تريد حفظ هذه اللعبة" },
    { "ARE YOU SURE YOU WANT TO DELETE THIS GAME", "APAKAH ANDA YAKIN INGIN MENGHAPUS PERMAINAN INI", "هل أنت متأكد أنك تريد حذف هذه اللعبة" },
    { "ARE YOU SURE YOU WANT TO DELETE THIS SAVE", "APAKAH ANDA YAKIN INGIN MENGHAPUS DATA SIMPAN INI", "هل أنت متأكد أنك تريد حذف هذا الحفظ" },
    { "GAME SAVED", "PERMAINAN DISIMPAN", "تم حفظ اللعبة" }, { "GAME LOADED", "PERMAINAN DIMUAT", "تم تحميل اللعبة" }, { "GAME DELETED", "PERMAINAN DIHAPUS", "تم حذف اللعبة" },
    { "SAVING", "MENYIMPAN", "جار الحفظ" }, { "LOADING", "MEMUAT", "جار التحميل" }, { "PLEASE WAIT", "HARAP TUNGGU", "يرجى الانتظار" },
    // language names (the Language page of the Indonesian / Arabic menu)
    { "ENGLISH", "INGGRIS", "الإنجليزية" }, { "FRENCH", "PRANCIS", "الفرنسية" }, { "FRANCAIS", "PRANCIS", "الفرنسية" }, { "GERMAN", "JERMAN", "الألمانية" }, { "DEUTSCH", "JERMAN", "الألمانية" },
    { "ITALIAN", "ITALIA", "الإيطالية" }, { "ITALIANO", "ITALIA", "الإيطالية" }, { "SPANISH", "SPANYOL", "الإسبانية" }, { "ESPANOL", "SPANYOL", "الإسبانية" },
    { "JAPANESE", "JEPANG", "اليابانية" }, { "RUSSIAN", "RUSIA", "الروسية" }, { "KOREAN", "KOREA", "الكورية" },
    // settings of other mods: Earshot ...
    { "REPLACE ORIGINAL SOUNDS", "GANTI SUARA ASLI", "استبدال الأصوات الأصلية" }, { "ORIGINAL SOUNDS", "SUARA ASLI", "الأصوات الأصلية" }, { "RICOCHET", "PANTULAN PELURU", "ارتداد الرصاص" },
    { "DISTANT SHOTS", "TEMBAKAN JAUH", "الطلقات البعيدة" }, { "DISTANT SHOTS FROM", "TEMBAKAN JAUH DARI", "طلقات بعيدة من" },
    { "HITMARKER", "PENANDA HIT", "علامة الإصابة" }, { "HITMARKERS", "PENANDA HIT", "علامات الإصابة" }, { "HIT MARKER", "PENANDA HIT", "علامة الإصابة" }, { "HIT MARKERS", "PENANDA HIT", "علامات الإصابة" },
    { "FOOTSTEPS", "LANGKAH KAKI", "الخطوات" }, { "NPC FOOTSTEPS", "LANGKAH KAKI NPC", "خطوات الشخصيات" }, { "MAX NPC FOOTSTEPS", "MAKS LANGKAH KAKI NPC", "الحد الأقصى لخطوات الشخصيات" },
    { "NPC", "NPC", "الشخصيات" }, { "NPCS", "NPC", "الشخصيات" }, { "SHOTS", "TEMBAKAN", "الطلقات" }, { "GUNSHOTS", "SUARA TEMBAKAN", "الطلقات النارية" }, { "EXPLOSIONS", "LEDAKAN", "الانفجارات" },
    // ... and the usual words of mod menus
    { "GRENADE AIM", "BIDIK GRANAT", "تصويب القنابل" }, { "GRENADE", "GRANAT", "القنبلة" }, { "AIM", "BIDIK", "التصويب" }, { "FPS COUNTER", "PENGHITUNG FPS", "عداد الإطارات" }, { "FPS", "FPS", "FPS" },
    { "WEAPON ICON", "IKON SENJATA", "أيقونة السلاح" }, { "WEAPON", "SENJATA", "السلاح" }, { "WEAPONS", "SENJATA", "الأسلحة" }, { "ICON", "IKON", "الأيقونة" }, { "ICONS", "IKON", "الأيقونات" },
    { "COLOR", "WARNA", "اللون" }, { "COLOUR", "WARNA", "اللون" }, { "COLORS", "WARNA", "الألوان" }, { "CROSSHAIR", "RETIKEL", "المؤشر" },
    { "SIZE", "UKURAN", "الحجم" }, { "SPEED", "KECEPATAN", "السرعة" }, { "SPEEDOMETER", "SPEEDOMETER", "عداد السرعة" }, { "HEALTH", "KESEHATAN", "الصحة" }, { "ARMOR", "ARMOR", "الدرع" }, { "ARMOUR", "ARMOR", "الدرع" },
    { "MONEY", "UANG", "المال" }, { "WANTED LEVEL", "TINGKAT BURONAN", "مستوى المطلوبية" }, { "VEHICLE", "KENDARAAN", "المركبة" }, { "VEHICLES", "KENDARAAN", "المركبات" }, { "CAR", "MOBIL", "السيارة" }, { "CARS", "MOBIL", "السيارات" },
    { "DAMAGE", "KERUSAKAN", "الضرر" }, { "TIME", "WAKTU", "الوقت" }, { "WEATHER", "CUACA", "الطقس" }, { "FOG", "KABUT", "الضباب" }, { "BLOOD", "DARAH", "الدم" }, { "PLAYER", "PEMAIN", "اللاعب" }, { "ENEMY", "MUSUH", "العدو" }, { "ENEMIES", "MUSUH", "الأعداء" },
    { "SOUND", "SUARA", "الصوت" }, { "SOUNDS", "SUARA", "الأصوات" }, { "VOLUME", "VOLUME", "مستوى الصوت" }, { "MENU", "MENU", "القائمة" }, { "BUTTON", "TOMBOL", "الزر" }, { "BUTTONS", "TOMBOL", "الأزرار" },
    { "POSITION", "POSISI", "الموضع" }, { "OPACITY", "OPASITAS", "الشفافية" }, { "TRANSPARENCY", "TRANSPARANSI", "الشفافية" }, { "SCALE", "SKALA", "المقياس" }, { "RELOAD", "ISI ULANG", "إعادة التعبئة" },
    { "DEBUG", "DEBUG", "التصحيح" }, { "INFO", "INFO", "المعلومات" }, { "INFORMATION", "INFORMASI", "المعلومات" }, { "RESET", "ATUR ULANG", "إعادة الضبط" }, { "APPLY", "TERAPKAN", "تطبيق" }, { "SAVE", "SIMPAN", "حفظ" }, { "LOAD", "MUAT", "تحميل" },
    { "CUSTOM", "KUSTOM", "مخصص" }, { "NORMAL", "NORMAL", "عادي" }, { "ORIGINAL", "ASLI", "الأصلي" }, { "REALISTIC", "REALISTIS", "واقعي" }, { "RED", "MERAH", "أحمر" }, { "GREEN", "HIJAU", "أخضر" }, { "BLUE", "BIRU", "أزرق" },
    { "YELLOW", "KUNING", "أصفر" }, { "WHITE", "PUTIH", "أبيض" }, { "BLACK", "HITAM", "أسود" }, { "MAX", "MAKS", "الحد الأقصى" }, { "MIN", "MIN", "الحد الأدنى" }, { "MAXIMUM", "MAKSIMUM", "الحد الأقصى" }, { "MINIMUM", "MINIMUM", "الحد الأدنى" },
    { "ENABLE", "AKTIFKAN", "تفعيل" }, { "DISABLE", "NONAKTIFKAN", "تعطيل" }, { "SHOW", "TAMPILKAN", "عرض" }, { "HIDE", "SEMBUNYIKAN", "إخفاء" }, { "ENABLE MOD", "AKTIFKAN MOD", "تفعيل المود" },
};
static const int kIdPhraseCount = (int)(sizeof(kIdPhrases) / sizeof(kIdPhrases[0]));
/* substrings of longer texts (save slot names: "AUTOSAVE: EMPTY SLOT") */
static const IdP kIdTokens[] = { { "EMPTY SLOT", "SLOT KOSONG", "خانة فارغة" }, { "CLOUD NOT AVAILABLE", "CLOUD TIDAK TERSEDIA", "السحابة غير متاحة" }, { "AUTOSAVE", "SIMPAN OTOMATIS", "حفظ تلقائي" } };

/* English names of the GXT keys the PC menu knows: used when the game's own text cannot be drawn (Cyrillic / Japanese), and as the
   wording Indonesian / Arabic are looked up by when the game itself is not English.
   kKeyEn: keys whose meaning is certain - they sit behind the stock Main menu, the Options tiles (tile -> key read from the stock
   screen: Game = FEH_LOA, Controls = FEC_RED, Display = FEO_DIS, Audio = FEO_AUD, Language = FEO_LAN) and the Language rows.
   kKeySoft: best guesses (setting names / values / Start Game tiles) that are only ever used instead of "????" - never to override
   a text the game gives in English. */
struct KeyEn { const char* key; const char* en; };
static const KeyEn kKeyEn[] = {
    { "FEP_RES", "RESUME" }, { "FEP_STG", "START GAME" }, { "FEP_OPT", "OPTIONS" }, { "FEP_QUI", "QUIT GAME" },
    { "FEH_STA", "STATS" }, { "FEH_BRI", "BRIEF" }, { "FEH_MAP", "MAP" },
    { "FEH_LOA", "GAME" }, { "FEC_RED", "CONTROLS" }, { "FEO_DIS", "DISPLAY" }, { "FEO_AUD", "AUDIO" }, { "FEO_LAN", "LANGUAGE" },
    { "FEH_DIS", "DISPLAY" }, { "FEH_AUD", "AUDIO" }, { "FEH_LAN", "LANGUAGE" }, { "FEO_GAM", "GAME" }, { "FEO_CON", "CONTROLS" },
    { "FEL_ENG", "ENGLISH" }, { "FEL_FRE", "FRENCH" }, { "FEL_GER", "GERMAN" }, { "FEL_ITA", "ITALIAN" }, { "FEL_SPA", "SPANISH" },
    { "FEL_RUS", "RUSSIAN" }, { "FEL_JPN", "JAPANESE" }, { "FEL_KOR", "KOREAN" },
    { "FEM_ON", "ON" }, { "FEM_OFF", "OFF" }, { "FEM_YES", "YES" }, { "FEM_NO", "NO" }, { "FEM_OK", "OK" },
};
static const KeyEn kKeySoft[] = {
    { "FED_BRI", "BRIGHTNESS" }, { "MOB_RES", "RESOLUTION" }, { "MOB_DD", "DRAW DISTANCE" }, { "FEM_FRM", "FRAME LIMITER" },
    { "FEA_SFX", "SFX VOLUME" }, { "FEA_MUS", "RADIO VOLUME" }, { "FEA_HRF", "HRTF" },
    { "MOB_SUB", "SUBTITLES" }, { "MOB_HUD", "HUD" }, { "MOB_VIB", "VIBRATION" }, { "MOB_RTD", "RESTORE DEFAULTS" },
    { "FES_NGA", "NEW GAME" }, { "FES_LOA", "LOAD GAME" }, { "FES_DEL", "DELETE GAME" },
    { "MOB_LOW", "LOW" }, { "MOB_HIG", "HIGH" }, { "MOB_STD", "STANDARD" }, { "MOB_ORG", "ORIGINAL" },
};
static const char* KeyLookup(const KeyEn* t, size_t n, const char* key)
{
    if(!key) return NULL;
    for(size_t i = 0; i < n; ++i) if(!strcmp(t[i].key, key)) return t[i].en;
    return NULL;
}
static const char* KeyEnglish(const char* key) { return KeyLookup(kKeyEn, sizeof(kKeyEn) / sizeof(kKeyEn[0]), key); }
static const char* KeyEnglishSoft(const char* key) { return KeyLookup(kKeySoft, sizeof(kKeySoft) / sizeof(kKeySoft[0]), key); }

/* upper case, single spaces, no trailing punctuation: the form phrases are compared in */
static std::string NormPhrase(const std::string& s, std::string* tail = NULL)
{
    std::string r; bool sp = false;
    for(size_t i = 0; i < s.size(); ++i)
    {
        unsigned char c = (unsigned char)s[i];
        if(c >= 'a' && c <= 'z') c -= 32;
        if(c == ' ' || c == '\t') { sp = !r.empty(); continue; }
        if(sp) { r += ' '; sp = false; }
        r += (char)c;
    }
    std::string t;
    while(!r.empty() && (r.back() == '.' || r.back() == ':' || r.back() == '!' || r.back() == '?' || r.back() == ' ')) { t.insert(t.begin(), r.back()); r.erase(r.size() - 1); }
    if(tail) *tail = t;
    return r;
}

/* PC menu fixed phrase (English upper-case key) in the PC menu language. English, Russian and Japanese stay English (the font has no glyphs for the latter two). */
static std::string Tr(const char* en)
{
    int lg = UiLang();
    if(lg == LG_EN || lg == LG_RU || lg == LG_JA) return en;
    for(int i = 0; i < kTxCount; ++i)
    {
        if(strcmp(kTx[i].en, en)) continue;
        const char* s = lg == LG_FR ? kTx[i].fr : lg == LG_DE ? kTx[i].de : lg == LG_IT ? kTx[i].it : lg == LG_ES ? kTx[i].es : lg == LG_ID ? kTx[i].id : lg == LG_AR ? kTx[i].ar : en;
        return (s && s[0]) ? std::string(s) : std::string(en);
    }
    return en;
}

/* Indonesian / Arabic for a text the game (or a mod) gave in English; empty when there is no translation.
   Order: a whole phrase of the tables; "ENABLE X" / "X VOLUME" style patterns around a part that is itself translated (or kept as it
   is: mod names, numbers); substrings (save slot names). */
struct IdPat { const char* word; bool prefix; const char* id; const char* ar; };       // prefix: "<word> X", else "X <word>" - becomes "<lead> X"
static const IdPat kIdPats[] = {
    { "ENABLE", true, "AKTIFKAN", "تفعيل" }, { "DISABLE", true, "NONAKTIFKAN", "تعطيل" }, { "SHOW", true, "TAMPILKAN", "عرض" }, { "HIDE", true, "SEMBUNYIKAN", "إخفاء" },
    { "VOLUME", false, "VOLUME", "مستوى صوت" }, { "SIZE", false, "UKURAN", "حجم" }, { "SPEED", false, "KECEPATAN", "سرعة" }, { "COLOR", false, "WARNA", "لون" },
    { "QUALITY", false, "KUALITAS", "جودة" }, { "SENSITIVITY", false, "SENSITIVITAS", "حساسية" }, { "OPACITY", false, "OPASITAS", "شفافية" },
    { "DISTANCE", false, "JARAK", "مسافة" }, { "COUNTER", false, "PENGHITUNG", "عداد" },
};
static bool PhraseExact(int lg, const std::string& n, std::string& out)
{
    for(int i = 0; i < kTxCount; ++i) if(n == kTx[i].en) { out = lg == LG_AR ? kTx[i].ar : kTx[i].id; return true; }
    for(int i = 0; i < kIdPhraseCount; ++i) if(n == kIdPhrases[i].en) { out = lg == LG_AR ? kIdPhrases[i].ar : kIdPhrases[i].id; return true; }
    return false;
}
static std::string PhraseSub(int lg, const std::string& n, int depth);
static bool PhrasePattern(int lg, const std::string& n, std::string& out, int depth)
{
    for(size_t k = 0; k < sizeof(kIdPats) / sizeof(kIdPats[0]); ++k)
    {
        const IdPat& p = kIdPats[k]; size_t wl = strlen(p.word); std::string rest;
        if(p.prefix) { if(n.size() > wl + 1 && n.compare(0, wl, p.word) == 0 && n[wl] == ' ') rest = n.substr(wl + 1); }
        else         { if(n.size() > wl + 1 && n.compare(n.size() - wl, wl, p.word) == 0 && n[n.size() - wl - 1] == ' ') rest = n.substr(0, n.size() - wl - 1); }
        if(rest.empty()) continue;
        out = std::string(lg == LG_AR ? p.ar : p.id) + " " + PhraseSub(lg, rest, depth + 1);
        return true;
    }
    return false;
}
static std::string PhraseSub(int lg, const std::string& n, int depth)            // translation of a part of a phrase; unknown parts stay as they are
{
    std::string o;
    if(PhraseExact(lg, n, o)) return o;
    if(depth < 3 && PhrasePattern(lg, n, o, depth)) return o;
    return n;
}
static std::string PhraseFor(int lg, const std::string& text)
{
    std::string tail, n = NormPhrase(text, &tail);
    if(n.empty() || (lg != LG_ID && lg != LG_AR)) return "";
    if(lg == LG_AR) { std::string t; for(size_t i = 0; i < tail.size(); ++i) t += (tail[i] == '?') ? "\xD8\x9F" : std::string(1, tail[i]); tail = t; }   // ? -> the Arabic question mark
    std::string o;
    if(PhraseExact(lg, n, o)) return o + tail;
    if(PhrasePattern(lg, n, o, 0)) return o + tail;
    std::string r = n; bool hit = false;                              // tokens inside a longer text
    for(size_t k = 0; k < sizeof(kIdTokens) / sizeof(kIdTokens[0]); ++k)
    {
        size_t at = r.find(kIdTokens[k].en);
        if(at != std::string::npos) { r.replace(at, strlen(kIdTokens[k].en), lg == LG_AR ? kIdTokens[k].ar : kIdTokens[k].id); hit = true; }
    }
    return hit ? r + tail : std::string();
}

/* "Menu De Pausa" -> "Menu de Pausa" after TitleCase: short connecting words stay small */
static std::string SmallWords(const std::string& t)
{
    static const char* small[] = { "De", "Di", "Du", "Des", "Del", "Der", "Die", "Das", "Und", "La", "Le", "Et", "Y", "E", "Dei", "Dal" };
    std::string r; size_t i = 0; bool first = true;
    while(i <= t.size())
    {
        size_t e = t.find(' ', i); if(e == std::string::npos) e = t.size();
        std::string w = t.substr(i, e - i);
        if(!first) for(size_t k = 0; k < sizeof(small) / sizeof(small[0]); ++k) if(w == small[k]) { w[0] = (char)(w[0] + 32); break; }
        if(!first) r += ' ';
        r += w; first = false;
        i = e + 1;
    }
    return r;
}

#endif
