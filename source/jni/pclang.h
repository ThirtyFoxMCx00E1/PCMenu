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

#include <map>
#include <set>
#include <unordered_map>
#include "pcdict.h"

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

/* ================================================================================================================
   Translation engine (v1.2.0): ONE lookup for every PC menu language (French, German, Italian, Spanish, Indonesian, Arabic).
   Any text the PC menu shows in English - its own labels (Tr), mod option rows that come as plain English text, new options
   added later - goes through PhraseFor(), so a new option is translated automatically:
     1. PCMenu/lang/<code>.txt  (user file, ENGLISH=TRANSLATION per line, wins over everything; fr de it es id ar)
     2. the whole phrase in the built-in tables (kTx, kDict from tools/pcdict.txt, kIdPhrases)
     3. a pattern around a known word ("ENABLE X", "X VOLUME", ...) with X translated again
     4. known parts of a save slot name (Indonesian / Arabic)
     5. word by word (longest known run of words first); words nobody knows (mod names, numbers) stay as they are
   Texts with no known word at all stay English and are written once to PCMenu/lang_missing.txt (LangMissingLog=0 turns it off),
   ready to be copied into lang/<code>.txt.
   ================================================================================================================ */
static int LangCol(int lg) { return lg == LG_FR ? 0 : lg == LG_DE ? 1 : lg == LG_IT ? 2 : lg == LG_ES ? 3 : lg == LG_ID ? 4 : lg == LG_AR ? 5 : -1; }
static const char* LangCode(int lg) { static const char* c[6] = { "fr", "de", "it", "es", "id", "ar" }; int k = LangCol(lg); return k < 0 ? "en" : c[k]; }
static const char* TxCol(const Tx& t, int lg) { switch(LangCol(lg)) { case 0: return t.fr; case 1: return t.de; case 2: return t.it; case 3: return t.es; case 4: return t.id; case 5: return t.ar; } return NULL; }
static const char* DictCol(const DictEntry& d, int lg) { switch(LangCol(lg)) { case 0: return d.fr; case 1: return d.de; case 2: return d.it; case 3: return d.es; case 4: return d.id; case 5: return d.ar; } return NULL; }

static bool g_cfgLangLog = true;                     // LangMissingLog= in pcmenu.ini
static std::map<std::string, std::string> g_userDict[6];
static bool g_userDictLoaded[6] = { false, false, false, false, false, false };
static int  g_userDictCount[6] = { 0, 0, 0, 0, 0, 0 };

/* UTF-8 Latin letters with accents -> plain ASCII (the PC fonts only have ASCII): "Paramètres" -> "Parametres" */
static std::string FoldLatinUtf8(const std::string& s)
{
    static const char* m40 = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";   // U+00C0..U+00FF
    std::string r;
    for(size_t i = 0; i < s.size(); ++i)
    {
        unsigned char c = (unsigned char)s[i];
        if(c == 0xC3 && i + 1 < s.size()) { unsigned char d = (unsigned char)s[i + 1]; if(d >= 0x80 && d <= 0xBF) { unsigned cp = 0xC0 + (d - 0x80); if(cp == 0xC6) r += "AE"; else if(cp == 0xDF) r += "SS"; else if(cp == 0xE6) r += "ae"; else r += m40[cp - 0xC0]; ++i; continue; } }
        if(c == 0xC5 && i + 1 < s.size()) { unsigned char d = (unsigned char)s[i + 1]; if(d == 0x92 || d == 0x93) { r += d == 0x92 ? "OE" : "oe"; ++i; continue; } }
        if(c == 0xE2 && i + 2 < s.size() && (unsigned char)s[i + 1] == 0x80 && ((unsigned char)s[i + 2] == 0x99 || (unsigned char)s[i + 2] == 0x98)) { r += '\''; i += 2; continue; }
        if(c >= 0x80) continue;                                // anything else the font cannot draw is dropped
        r += (char)c;
    }
    return r;
}
static std::string NormPhrase(const std::string& s, std::string* tail = NULL);
static void LoadUserDict(int col)
{
    if(col < 0 || col > 5 || g_userDictLoaded[col]) return;
    g_userDictLoaded[col] = true;
    static const char* codes[6] = { "fr", "de", "it", "es", "id", "ar" };
    std::string path = g_dirPC + "lang/" + codes[col] + ".txt";
    FILE* f = fopen(path.c_str(), "rb"); if(!f) return;
    std::string txt; char buf[1024]; size_t n; while((n = fread(buf, 1, sizeof(buf), f)) > 0) txt.append(buf, n); fclose(f);
    if(txt.size() >= 3 && (unsigned char)txt[0] == 0xEF && (unsigned char)txt[1] == 0xBB && (unsigned char)txt[2] == 0xBF) txt.erase(0, 3);
    size_t pos = 0;
    while(pos < txt.size())
    {
        size_t e = txt.find('\n', pos); if(e == std::string::npos) e = txt.size();
        std::string line = txt.substr(pos, e - pos); pos = e + 1;
        while(!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t b = line.find_first_not_of(" \t"); if(b == std::string::npos) continue; line.erase(0, b);
        if(line[0] == '#' || line[0] == ';') continue;
        size_t eq = line.find('='); if(eq == std::string::npos || eq == 0) continue;
        std::string k = NormPhrase(line.substr(0, eq)), v = line.substr(eq + 1);
        while(!v.empty() && (v[0] == ' ' || v[0] == '\t')) v.erase(0, 1);
        if(col != 5) v = Upper(FoldLatinUtf8(v));          // Latin languages / Indonesian: ASCII upper case like the tables
        if(k.empty() || v.empty()) continue;
        g_userDict[col][k] = v; ++g_userDictCount[col];
    }
}
static std::set<std::string> g_missSeen; static bool g_missInit = false;
static void NoteUntranslated(int lg, const std::string& n)
{
    if(!g_cfgLangLog || g_dirPC.empty()) return;
    int letters = 0; for(size_t i = 0; i < n.size(); ++i) if(n[i] >= 'A' && n[i] <= 'Z') ++letters;
    if(letters < 2) return;                                                  // numbers, "50%", single letters
    if(n.find('_') != std::string::npos && n.find(' ') == std::string::npos) return;   // a GXT key the game does not have ("MOB_SFX"), not a text
    std::string line = std::string(LangCode(lg)) + ": " + n + "=";
    std::string path = g_dirPC + "lang_missing.txt";
    if(!g_missInit)
    {
        g_missInit = true;
        FILE* f = fopen(path.c_str(), "rb");
        if(f) { char b[512]; while(fgets(b, sizeof(b), f)) { std::string l = b; while(!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back(); g_missSeen.insert(l); } fclose(f); }
    }
    if(g_missSeen.size() > 2000 || !g_missSeen.insert(line).second) return;
    FILE* f = fopen(path.c_str(), "ab"); if(!f) return;
    if(ftell(f) == 0) fputs("# PCMenu: texts that had no translation (language: ENGLISH=). Copy a line without the \"xx: \" prefix into PCMenu/lang/xx.txt and write the translation after '='.\n", f);
    fputs((line + "\n").c_str(), f); fclose(f);
}

/* upper case, single spaces, no trailing punctuation: the form phrases are compared in */
static std::string NormPhrase(const std::string& s, std::string* tail)
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

static bool PhraseExact(int lg, const std::string& n, std::string& out)
{
    int col = LangCol(lg); if(col < 0) return false;
    LoadUserDict(col);
    if(g_userDictCount[col]) { std::map<std::string, std::string>::const_iterator it = g_userDict[col].find(n); if(it != g_userDict[col].end()) { out = it->second; return true; } }
    for(int i = 0; i < kTxCount; ++i) if(n == kTx[i].en) { const char* s = TxCol(kTx[i], lg); if(s && s[0]) { out = s; return true; } }
    for(int i = 0; i < kDictCount; ++i) if(n == kDict[i].en) { const char* s = DictCol(kDict[i], lg); if(s && s[0]) { out = s; return true; } break; }
    if(lg == LG_ID || lg == LG_AR)
        for(int i = 0; i < kIdPhraseCount; ++i) if(n == kIdPhrases[i].en) { out = lg == LG_AR ? kIdPhrases[i].ar : kIdPhrases[i].id; return true; }
    return false;
}

/* "<word> X" (prefix) or "X <word>": the translation is a template, %s = X translated again */
struct Pat { const char* word; bool prefix; const char* t[6]; };      // fr de it es id ar
static const Pat kPats[] = {
    { "ENABLE",  true,  { "ACTIVER %s", "%s AKTIVIEREN", "ATTIVA %s", "ACTIVAR %s", "AKTIFKAN %s", "\xD8\xAA\xD9\x81\xD8\xB9\xD9\x8A\xD9\x84 %s" } },
    { "DISABLE", true,  { "DESACTIVER %s", "%s DEAKTIVIEREN", "DISATTIVA %s", "DESACTIVAR %s", "NONAKTIFKAN %s", "\xD8\xAA\xD8\xB9\xD8\xB7\xD9\x8A\xD9\x84 %s" } },
    { "SHOW",    true,  { "AFFICHER %s", "%s ANZEIGEN", "MOSTRA %s", "MOSTRAR %s", "TAMPILKAN %s", "\xD8\xB9\xD8\xB1\xD8\xB6 %s" } },
    { "HIDE",    true,  { "MASQUER %s", "%s AUSBLENDEN", "NASCONDI %s", "OCULTAR %s", "SEMBUNYIKAN %s", "\xD8\xA5\xD8\xAE\xD9\x81\xD8\xA7\xD8\xA1 %s" } },
    { "USE",     true,  { "UTILISER %s", "%s VERWENDEN", "USA %s", "USAR %s", "GUNAKAN %s", "\xD8\xA7\xD8\xB3\xD8\xAA\xD8\xAE\xD8\xAF\xD8\xA7\xD9\x85 %s" } },
    { "VOLUME",  false, { "VOLUME %s", "%s-LAUTSTARKE", "VOLUME %s", "VOLUMEN DE %s", "VOLUME %s", "\xD9\x85\xD8\xB3\xD8\xAA\xD9\x88\xD9\x89 \xD8\xB5\xD9\x88\xD8\xAA %s" } },
    { "SIZE",    false, { "TAILLE %s", "%s-GROSSE", "DIMENSIONE %s", "TAMANO DE %s", "UKURAN %s", "\xD8\xAD\xD8\xAC\xD9\x85 %s" } },
    { "SPEED",   false, { "VITESSE %s", "%s-GESCHWINDIGKEIT", "VELOCITA %s", "VELOCIDAD DE %s", "KECEPATAN %s", "\xD8\xB3\xD8\xB1\xD8\xB9\xD8\xA9 %s" } },
    { "COLOR",   false, { "COULEUR %s", "%s-FARBE", "COLORE %s", "COLOR DE %s", "WARNA %s", "\xD9\x84\xD9\x88\xD9\x86 %s" } },
    { "QUALITY", false, { "QUALITE %s", "%s-QUALITAT", "QUALITA %s", "CALIDAD DE %s", "KUALITAS %s", "\xD8\xAC\xD9\x88\xD8\xAF\xD8\xA9 %s" } },
    { "SENSITIVITY", false, { "SENSIBILITE %s", "%s-EMPFINDLICHKEIT", "SENSIBILITA %s", "SENSIBILIDAD DE %s", "SENSITIVITAS %s", "\xD8\xAD\xD8\xB3\xD8\xA7\xD8\xB3\xD9\x8A\xD8\xA9 %s" } },
    { "OPACITY", false, { "OPACITE %s", "%s-DECKKRAFT", "OPACITA %s", "OPACIDAD DE %s", "OPASITAS %s", "\xD8\xB4\xD9\x81\xD8\xA7\xD9\x81\xD9\x8A\xD8\xA9 %s" } },
    { "DISTANCE", false, { "DISTANCE %s", "%s-ENTFERNUNG", "DISTANZA %s", "DISTANCIA DE %s", "JARAK %s", "\xD9\x85\xD8\xB3\xD8\xA7\xD9\x81\xD8\xA9 %s" } },
    { "COUNTER", false, { "COMPTEUR %s", "%s-ZAHLER", "CONTATORE %s", "CONTADOR DE %s", "PENGHITUNG %s", "\xD8\xB9\xD8\xAF\xD8\xA7\xD8\xAF %s" } },
    { "MODE",    false, { "MODE %s", "%s-MODUS", "MODALITA %s", "MODO %s", "MODE %s", "\xD9\x88\xD8\xB6\xD8\xB9 %s" } },
};
static std::string PhraseSub(int lg, const std::string& n, int depth);
static bool PhrasePattern(int lg, const std::string& n, std::string& out, int depth)
{
    int col = LangCol(lg); if(col < 0) return false;
    for(size_t k = 0; k < sizeof(kPats) / sizeof(kPats[0]); ++k)
    {
        const Pat& p = kPats[k]; size_t wl = strlen(p.word); std::string rest;
        if(p.prefix) { if(n.size() > wl + 1 && n.compare(0, wl, p.word) == 0 && n[wl] == ' ') rest = n.substr(wl + 1); }
        else         { if(n.size() > wl + 1 && n.compare(n.size() - wl, wl, p.word) == 0 && n[n.size() - wl - 1] == ' ') rest = n.substr(0, n.size() - wl - 1); }
        if(rest.empty()) continue;
        std::string tpl = p.t[col], sub = PhraseSub(lg, rest, depth + 1);
        size_t at = tpl.find("%s"); out = at == std::string::npos ? tpl + " " + sub : tpl.substr(0, at) + sub + tpl.substr(at + 2);
        return true;
    }
    return false;
}
/* word by word: the longest run of words (up to 4) that is a known phrase is replaced; unknown words stay */
static std::string WordByWord(int lg, const std::string& n, int* hits)
{
    std::vector<std::string> w; size_t i = 0;
    while(i < n.size()) { size_t e = n.find(' ', i); if(e == std::string::npos) e = n.size(); if(e > i) w.push_back(n.substr(i, e - i)); i = e + 1; }
    std::string r; int h = 0;
    for(size_t k = 0; k < w.size();)
    {
        size_t take = 0; std::string o;
        for(size_t len = std::min((size_t)4, w.size() - k); len >= 1 && !take; --len)
        {
            std::string span = w[k]; for(size_t j = 1; j < len; ++j) span += " " + w[k + j];
            if(PhraseExact(lg, span, o)) take = len;
            else if(len == 1)                                                // "(BETA)" / "SPEED:" -> the word inside its punctuation
            {
                size_t a = span.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), b = span.find_last_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
                if(a != std::string::npos && (a > 0 || b + 1 < span.size()) && PhraseExact(lg, span.substr(a, b - a + 1), o)) { o = span.substr(0, a) + o + span.substr(b + 1); take = 1; }
            }
        }
        if(!r.empty()) r += ' ';
        if(take) { r += o; ++h; k += take; } else { r += w[k]; ++k; }
    }
    if(hits) *hits = h;
    return r;
}
static std::string PhraseSub(int lg, const std::string& n, int depth)            // translation of a part of a phrase; unknown parts stay as they are
{
    std::string o;
    if(PhraseExact(lg, n, o)) return o;
    if(depth < 3 && PhrasePattern(lg, n, o, depth)) return o;
    int h = 0; std::string w = WordByWord(lg, n, &h);
    return h ? w : n;
}
/* the text in language lg; "" when nothing of it is known. words = false: whole phrases / patterns / slot names only (no word-by-word, no log) */
static std::string PhraseFor(int lg, const std::string& text, bool words = true)
{
    if(LangCol(lg) < 0) return "";
    std::string tail, n = NormPhrase(text, &tail);
    if(n.empty()) return "";
    if(lg == LG_AR) { std::string t; for(size_t i = 0; i < tail.size(); ++i) t += (tail[i] == '?') ? "\xD8\x9F" : std::string(1, tail[i]); tail = t; }   // ? -> the Arabic question mark
    std::string o;
    if(PhraseExact(lg, n, o)) return o + tail;
    if(PhrasePattern(lg, n, o, 0)) return o + tail;
    if(lg == LG_ID || lg == LG_AR)
    {
        std::string r = n; bool hit = false;                              // tokens inside a longer text
        for(size_t k = 0; k < sizeof(kIdTokens) / sizeof(kIdTokens[0]); ++k)
        {
            size_t at = r.find(kIdTokens[k].en);
            if(at != std::string::npos) { r.replace(at, strlen(kIdTokens[k].en), lg == LG_AR ? kIdTokens[k].ar : kIdTokens[k].id); hit = true; }
        }
        if(hit) return r + tail;
    }
    if(!words) return "";
    int h = 0; std::string w = WordByWord(lg, n, &h);
    if(h) return w + tail;
    NoteUntranslated(lg, n);
    return "";
}

/* PC menu phrase (English upper-case) in the PC menu language. English, Russian and Japanese stay English (the font has no glyphs for
   the latter two). Phrases not in any table are still translated (word by word), so new labels need no code change. */
static std::unordered_map<std::string, std::string> g_trCache;
static std::string Tr(const char* en)
{
    int lg = UiLang();
    if(LangCol(lg) < 0 || !en || !en[0]) return en ? en : "";
    std::string key = std::string(1, (char)('0' + LangCol(lg))) + en;
    std::unordered_map<std::string, std::string>::const_iterator it = g_trCache.find(key);
    if(it != g_trCache.end()) return it->second;
    std::string o = PhraseFor(lg, en);
    if(o.empty()) o = en;
    if(g_trCache.size() > 4000) g_trCache.clear();
    g_trCache[key] = o;
    return o;
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
