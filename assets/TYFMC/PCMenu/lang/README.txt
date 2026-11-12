PCMenu language files (1.2.0)
=============================

Put a file here named after the language code:
  fr.txt  French      de.txt  German     it.txt  Italian
  es.txt  Spanish     id.txt  Indonesian ar.txt  Arabic (UTF-8)

One line per text:   ENGLISH TEXT=TRANSLATION
Lines starting with ; or # are comments. Case does not matter on the left side.
Accented letters (UTF-8) are fine; they are drawn with the menu font.

Your lines win over the built-in dictionary, so you can fix any wording.
Any option a mod adds is translated automatically:
  1. your file   2. built-in dictionary   3. patterns such as "ENABLE %s" or "%s VOLUME"
  4. word by word (known words are translated, unknown ones stay)

With LangMissingLog=1 the menu writes ../lang_missing.txt: every text it could not
translate. Copy lines from there into your file and add the translation.
