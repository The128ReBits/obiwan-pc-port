/* configurador_obiwan_textos.h -- textos de la ventana de "Configurar Obi-Wan.exe"
 * (Claude completo-4, 2026-11-06). UTF-8, compilar con /utf-8.
 *
 * Una fila por texto: TX(identificador, "español", "english").
 * Para añadir un idioma:
 *   1. añade una columna más a TODAS las filas de abajo (mismo orden que TX_IDIOMAS),
 *   2. añade su código a TX_IDIOMAS,
 *   3. añade el código a la lista de [Language] interfaz en tools/obiwan_ajustes.h
 *      ("es;en" -> "es;en;fr") y su nombre en las dos columnas de opciones.
 * No hace falta tocar el código. Los textos de cada ajuste (nombre y ayuda) viven
 * en tools/obiwan_ajustes.h (es y en); un idioma sin columna allí usa el inglés.
 * Los %ls, %d, %lu... son huecos que rellena el programa: mantenlos en el mismo orden. */

#ifndef TX_IDIOMAS
#define TX_IDIOMAS "es", "en"
#endif

#ifdef TX
/* ---- ventana ---- */
TX(T_TITULO,            "Configurar Obi-Wan", "Configure Obi-Wan")
TX(T_SUBTITULO,         "Star Wars: Obi-Wan · port de PC hecho por aficionados", "Star Wars: Obi-Wan · fan-made PC port")
TX(T_NAV_JUGAR,         "Jugar", "Play")
TX(T_NAV_PANTALLA,      "Pantalla", "Display")
TX(T_NAV_GRAFICOS,      "Gráficos", "Graphics")
TX(T_NAV_AUDIO,         "Audio", "Audio")
TX(T_NAV_CONTROLES,     "Controles", "Controls")
TX(T_NAV_IDIOMA,        "Idioma", "Language")
TX(T_NAV_HERRAMIENTAS,  "Herramientas", "Tools")
TX(T_NAV_DEV,           "Desarrollador", "Developer")
TX(T_NAV_CREDITOS,      "Créditos y licencias", "Credits and licences")
TX(T_GUARDAR,           "Guardar", "Save")
TX(T_DESCARTAR,         "Descartar cambios", "Discard changes")
TX(T_CERRAR,            "Cerrar", "Close")
TX(T_SIN_CAMBIOS,       "Sin cambios por guardar.", "No unsaved changes.")
TX(T_HAY_CAMBIOS,       "Hay cambios sin guardar (Ctrl+S guarda).", "There are unsaved changes (Ctrl+S saves).")
TX(T_LEIDOS,            "Ajustes leídos.", "Settings loaded.")
TX(T_DESCARTADOS,       "Cambios descartados.", "Changes discarded.")
TX(T_GUARDADO,          "Guardado en %ls.", "Saved to %ls.")
TX(T_GUARDADO_LOCAL,    "Windows no dejó escribir en %ls (error %lu; puede ser el Acceso controlado a carpetas). Guardado en %ls, que el lanzador también lee.",
                        "Windows did not allow writing to %ls (error %lu; it may be Controlled folder access). Saved to %ls, which the launcher also reads.")
TX(T_NO_GUARDADO,       "No pude guardar los ajustes (error %lu).", "Could not save the settings (error %lu).")
TX(T_PREG_SALIR,        "Hay cambios sin guardar. ¿Guardarlos antes de cerrar?", "There are unsaved changes. Save them before closing?")
TX(T_RECOMENDADO,       "Recomendado: ", "Recommended: ")
TX(T_RECOMENDADO_SUF,   " (recomendado)", " (recommended)")
TX(T_VACIO_DEFECTO,     "vacío (lo de siempre)", "empty (the usual)")
TX(T_ELEGIR_CARPETA,    "Elegir…", "Browse…")
TX(T_VALOR_NO_VALE,     "«%ls» no vale aquí (%ls). Al salir del campo vuelve el valor anterior.", "“%ls” is not valid here (%ls). Leaving the field restores the previous value.")
TX(T_RANGO,             "de %ls a %ls", "from %ls to %ls")
TX(T_TEXTO_MAL,         "texto no válido", "invalid text")
TX(T_NINGUNA,           "(ninguna)", "(none)")
TX(T_TECLA_REPETIDA,    "Ojo: esta tecla también está en «%ls».", "Note: this key is also used for “%ls”.")
TX(T_SI,                "sí", "yes")
TX(T_NO,                "no", "no")
TX(T_ERROR,             "Error", "Error")
TX(T_AVISO,             "Aviso", "Warning")
TX(T_OCUPADO,           "Ya hay una tarea en marcha: espera a que termine.", "A task is already running: wait for it to finish.")
TX(T_TAREA_EN_MARCHA,   "Hay una tarea en marcha. ¿Cerrar de todos modos? (se cancela)", "A task is running. Close anyway? (it will be cancelled)")
TX(T_CANCELADO,         "Cancelado.", "Cancelled.")
TX(T_JUEGO_ABIERTO_AVISO,"El juego está abierto: los cambios se notarán la próxima vez que lo abras.", "The game is running: changes apply the next time you start it.")

/* ---- Jugar ---- */
TX(T_JUGAR,             "Jugar", "Play")
TX(T_JUGAR_MEDIR,       "Jugar y medir", "Play and measure")
TX(T_JUGAR_AYUDA,       "Jugar guarda los ajustes y abre «Jugar Obi-Wan 6.exe». Jugar y medir hace lo mismo que «MEDIR TODO - Obi-Wan 6.bat»: juega con las medidas encendidas y, al cerrar el juego, abre la carpeta con lo medido.",
                        "Play saves the settings and opens “Jugar Obi-Wan 6.exe”. Play and measure does the same as “MEDIR TODO - Obi-Wan 6.bat”: it plays with the measurements on and, when the game closes, opens the folder with the results.")
TX(T_LISTO,             "Listo para jugar", "Ready to play")
TX(T_NO_LISTO,          "Falta algo para jugar: mira lo que sale en rojo", "Something is missing: look at the red lines")
TX(T_ESTADO_JUEGO,      "Estado del juego", "Game status")
TX(T_LANZADOR,          "Lanzador", "Launcher")
TX(T_COMPILADO,         "Juego compilado", "Built game")
TX(T_EDICION,           "Edición", "Edition")
TX(T_CARPETA_JUEGO,     "Carpeta del juego", "Game folder")
TX(T_FIRMA,             "Firma del default.xbe", "default.xbe signature")
TX(T_ARCHIVOS,          "Archivos del juego", "Game files")
TX(T_PARTIDAS,          "Partidas", "Saves")
TX(T_AJUSTES,           "Ajustes", "Settings")
TX(T_ABIERTO,           "Juego abierto ahora", "Game running now")
TX(T_ENCONTRADO,        "%ls (encontrado)", "%ls (found)")
TX(T_FALTA,             "%ls (falta)", "%ls (missing)")
TX(T_COMPILADO_V,       "%ls · %.1f MB · %ls", "%ls · %.1f MB · %ls")
TX(T_FIRMA_OK,          "%ls (firma correcta)", "%ls (signature matches)")
TX(T_FIRMA_DESCONOCIDA, "desconocida: no es de ninguna edición conocida", "unknown: it matches no known edition")
TX(T_FIRMA_PAL,         "%ls: falta compilar la edición PAL", "%ls: the PAL edition is not built yet")
TX(T_FIRMA_NO_XBE,      "no hay default.xbe", "there is no default.xbe")
TX(T_SIN_CARPETA,       "no hay carpeta del juego (usa Idioma > Importar ISO)", "no game folder (use Language > Import ISO)")
TX(T_ARCH_OK,           "%d archivos con el tamaño correcto (comprobación rápida; la completa está en Herramientas)", "%d files with the right size (quick check; the full one is in Tools)")
TX(T_ARCH_MAL,          "%d de %d archivos cambiados o que faltan: usa Herramientas > Verificar", "%d of %d files changed or missing: use Tools > Verify")
TX(T_ARCH_SIN,          "sin EXTRACCION.json: no puedo comprobar los archivos", "no EXTRACCION.json: the files cannot be checked")
TX(T_PART_N,            "%d archivos en %ls", "%d files in %ls")
TX(T_PART_VACIA,        "todavía no hay partidas (%ls)", "no saves yet (%ls)")
TX(T_INI_NO,            "sin obiwan.ini: todo por defecto, igual que el lanzador 5", "no obiwan.ini: all defaults, same as launcher 5")
TX(T_INI_N,             "%d ajustes distintos del de por defecto (%ls)", "%d settings differ from the default (%ls)")
TX(T_INI_DEFECTO,       "todo por defecto (%ls)", "all defaults (%ls)")
TX(T_INI_AVISOS,        "obiwan.ini tiene %d avisos (valores que no valen): se usan los de por defecto", "obiwan.ini has %d warnings (invalid values): the defaults are used")
TX(T_ABIERTO_SI,        "sí: ciérralo antes de medir o importar", "yes: close it before measuring or importing")
TX(T_ULTIMA,            "Última partida", "Last game")
TX(T_ULT_NINGUNA,       "Todavía no hay ninguna partida con el lanzador 6.", "No game with launcher 6 yet.")
TX(T_ULT_FECHA,         "Fecha", "Date")
TX(T_ULT_RESULTADO,     "Resultado", "Result")
TX(T_ULT_BIEN,          "cerró bien (código 0)", "closed fine (code 0)")
TX(T_ULT_ERROR,         "terminó con error (código %lu)", "ended with an error (code %lu)")
TX(T_ULT_SIN,           "sin línea de cierre: sigue abierto o se cortó", "no closing line: still running or cut off")
TX(T_ULT_FPS,           "Imágenes por segundo (media)", "Frames per second (average)")
TX(T_ULT_FPS_V,         "%.1f (media de %d medidas cada pocos segundos)", "%.1f (average of %d readings a few seconds apart)")
TX(T_SIN_DATOS,         "sin datos", "no data")
TX(T_ULT_TIRONES,       "Tirones", "Stutters")
TX(T_ULT_TIRONES_V,     "%lu imágenes de más de 33 ms (%lu de más de 50 ms) de %lu", "%lu frames over 33 ms (%lu over 50 ms) out of %lu")
TX(T_ULT_TIRONES_NO,    "sin datos: usa Jugar y medir", "no data: use Play and measure")
TX(T_ULT_MODO,          "Modo", "Mode")
TX(T_ABRIR_LOG,         "Abrir log", "Open log")
TX(T_ABRIR_LOGS,        "Abrir carpeta de logs", "Open logs folder")
TX(T_COMPROBAR,         "Comprobar de nuevo", "Check again")
TX(T_JUEGO_YA_ABIERTO,  "El juego ya está abierto.", "The game is already running.")
TX(T_NO_ENCUENTRO,      "No encuentro «%ls».", "Cannot find “%ls”.")
TX(T_ABIERTO_OK,        "Abierto «%ls».", "Opened “%ls”.")

/* ---- Pantalla ---- */
TX(T_NOTA_PANTALLA,     "No existe pantalla completa exclusiva en este port: «Pantalla completa sin bordes» cubre todo el monitor y es lo más parecido. En el juego, F11 o Alt+Intro cambian entre ventana y pantalla completa. Por defecto el juego marca su propio ritmo, como hasta ahora.",
                        "This port has no exclusive fullscreen: “Borderless fullscreen” covers the whole monitor and is the closest thing. In game, F11 or Alt+Enter switch between window and fullscreen. By default the game keeps its own pacing, as until now.")
TX(T_MONITORES,         "Monitores que ve Windows: %ls", "Monitors Windows sees: %ls")
TX(T_PRINCIPAL,         "principal", "main")

/* ---- Gráficos ---- */
TX(T_NOTA_ULTRA,        "Texturas Ultra: no salen porque no existe un pack de texturas para Obi-Wan (el código de texturas Ultra viene de otro juego y su pack es de ese juego).",
                        "Ultra textures: not shown because no texture pack exists for Obi-Wan (the Ultra texture code comes from another game and its pack belongs to that game).")
TX(T_NOTA_ARREGLOS,     "Los arreglos gráficos (TEX_LOD, SHADOW_PCF…) van encendidos y solo se pueden apagar en Desarrollador, para comparar.",
                        "The graphics fixes (TEX_LOD, SHADOW_PCF…) are on and can only be turned off in Developer, to compare.")
TX(T_SEC_CALIDAD,       "Calidad de imagen", "Image quality")
TX(T_SEC_AVANZADO,      "Avanzado", "Advanced")

/* ---- Audio ---- */
TX(T_PROBAR_SONIDO,     "Probar sonido", "Test sound")
TX(T_PROBAR_SONIDO_AYUDA,"Toca un tono corto en la salida y con el volumen elegidos (aunque no estén guardados). No usa el juego.",
                        "Plays a short tone on the chosen output at the chosen volume (even if not saved). It does not use the game.")
TX(T_PREDETERMINADA,    "Predeterminada de Windows", "Windows default")
TX(T_NO_CONECTADA,      "%ls (no conectada)", "%ls (not connected)")
TX(T_SONIDO_OK,         "Sonó en: %ls, al %d %%.", "Played on: %ls, at %d %%.")
TX(T_SONIDO_MAL,        "No pude tocar el tono (error 0x%08lX).", "Could not play the tone (error 0x%08lX).")
TX(T_SONIDO_SONANDO,    "Sonando…", "Playing…")
TX(T_NOTA_VOLUMENES,    "Volumen de música, voces y efectos por separado: no se puede. El juego los mezcla en su chip de sonido (emulado) antes de que lleguen al PC.",
                        "Separate music, voice and effects volume: not possible. The game mixes them in its (emulated) sound chip before they reach the PC.")

/* ---- Controles ---- */
TX(T_SEC_MANDO,         "Mando", "Controller")
TX(T_SEC_RATON,         "Ratón", "Mouse")
TX(T_SEC_TECLAS,        "Teclas", "Keys")
TX(T_PROBAR_MANDO,      "Probar mando", "Test controller")
TX(T_PARAR_MANDO,       "Parar la prueba", "Stop the test")
TX(T_PROBAR_MANDO_AYUDA,"Solo lee el mando: no vibra ni cambia nada. Solo con el juego cerrado.", "Only reads the controller: no rumble, nothing changes. Only with the game closed.")
TX(T_MANDO_NINGUNO,     "No veo ningún mando conectado.", "No controller connected.")
TX(T_MANDO_JUEGO,       "Cierra el juego para probar el mando.", "Close the game to test the controller.")
TX(T_MANDO_XINPUT,      "Mando %d: stick izq. %+6d %+6d · stick der. %+6d %+6d · gatillos %3d %3d · botones: %ls",
                        "Pad %d: left stick %+6d %+6d · right stick %+6d %+6d · triggers %3d %3d · buttons: %ls")
TX(T_MANDO_JOY,         "Mando no XInput %d: X %5lu Y %5lu Z %5lu R %5lu · botones %08lX", "Non-XInput pad %d: X %5lu Y %5lu Z %5lu R %5lu · buttons %08lX")
TX(T_MKB_INI,           "Hay un mkb.ini en %ls. El juego lo lee después de estos ajustes y le gana: lo que cambies aquí del teclado y el ratón puede no notarse. Bórralo o cámbiale el nombre para usar esta página.",
                        "There is an mkb.ini in %ls. The game reads it after these settings and it wins: what you change here for keyboard and mouse may not apply. Delete or rename it to use this page.")
TX(T_NOTA_TECLAS,       "F9, F11 y F12 están reservadas. Los botones del ratón son fijos: izquierdo = gatillo derecho, derecho = gatillo izquierdo, central o rueda = Y, laterales = blanco y negro.",
                        "F9, F11 and F12 are reserved. Mouse buttons are fixed: left = right trigger, right = left trigger, middle or wheel = Y, side buttons = white and black.")

/* ---- Idioma ---- */
TX(T_SEC_EDICIONES,     "Ediciones del juego", "Game editions")
TX(T_NOTA_EDICIONES,    "Cada idioma del juego es una edición distinta, con su propio default.xbe (se reconoce por su firma). Hoy solo la de EE. UU. está compilada; las PAL se ven pero no se pueden elegir hasta compilarlas.",
                        "Each game language is a different edition, with its own default.xbe (recognised by its signature). Today only the USA one is built; the PAL ones are listed but cannot be chosen until they are built.")
TX(T_COL_IDIOMA,        "Idioma", "Language")
TX(T_COL_FIRMA,         "Firma", "Signature")
TX(T_COL_ESTADO,        "Estado", "Status")
TX(T_COL_CARPETA,       "Archivos", "Files")
TX(T_ED_LISTA,          "Lista para jugar", "Ready to play")
TX(T_ED_FALTA,          "Falta compilar la edición PAL", "The PAL edition is not built yet")
TX(T_ED_CARPETA_SI,     "importada (%ls)", "imported (%ls)")
TX(T_ED_CARPETA_NO,     "no importada", "not imported")
TX(T_IMPORTAR,          "Importar ISO…", "Import ISO…")
TX(T_IMPORTAR_AYUDA,    "Copia los archivos de tu propia ISO a private\\game\\<edición>. La ISO solo se lee. La edición se reconoce por la firma del default.xbe; una ISO desconocida se rechaza, y nunca se sobrescribe una carpeta que ya tenga archivos.",
                        "Copies the files of your own ISO to private\\game\\<edition>. The ISO is only read. The edition is recognised by the default.xbe signature; an unknown ISO is refused, and a folder that already has files is never overwritten.")
TX(T_IMP_LEYENDO,       "Leyendo la ISO…", "Reading the ISO…")
TX(T_IMP_COPIANDO,      "Copiando %d de %d (%ls)…", "Copying %d of %d (%ls)…")
TX(T_IMP_VERIFICANDO,   "Comprobando lo copiado…", "Checking the copy…")
TX(T_IMP_OK,            "Importada la edición %ls: %d archivos en %ls.", "Imported the %ls edition: %d files in %ls.")
TX(T_IMP_ERR,           "No se importó: %ls", "Not imported: %ls")
TX(T_E_ABRIR,           "no puedo abrir la ISO", "cannot open the ISO")
TX(T_E_NO_XDVDFS,       "no es una imagen de disco de Xbox (no hay sistema de archivos XDVDFS)", "not an Xbox disc image (no XDVDFS file system)")
TX(T_E_SIN_XBE,         "la ISO no tiene default.xbe", "the ISO has no default.xbe")
TX(T_E_DESCONOCIDA,     "la firma del default.xbe no es de ninguna edición conocida de Obi-Wan", "the default.xbe signature matches no known edition of Obi-Wan")
TX(T_E_YA_IMPORTADA,    "esa edición ya está importada en %ls (tiene EXTRACCION.json)", "that edition is already imported in %ls (it has EXTRACCION.json)")
TX(T_E_DESTINO,         "la carpeta de destino ya tiene archivos (%ls): no sobrescribo nada", "the destination folder already has files (%ls): nothing is overwritten")
TX(T_E_TEMPORAL,        "quedó una importación a medias en %ls: bórrala o muévela antes de repetir", "a half-done import is left in %ls: delete or move it before retrying")
TX(T_E_ESPACIO,         "no hay sitio: hacen falta %.1f GB y hay %.1f GB libres", "not enough space: %.1f GB needed, %.1f GB free")
TX(T_E_LECTURA,         "error leyendo la ISO", "error reading the ISO")
TX(T_E_ESCRITURA,       "error escribiendo %ls (error %lu)", "error writing %ls (error %lu)")
TX(T_E_ISO_CAMBIO,      "la ISO cambió mientras la leía", "the ISO changed while it was being read")
TX(T_E_ESTRUCTURA,      "la ISO tiene una estructura rara (%ls)", "the ISO has an odd structure (%ls)")
TX(T_E_VERIF,           "lo copiado no coincide con la ISO", "the copy does not match the ISO")
TX(T_FILTRO_ISO,        "Imagen de disco (*.iso)", "Disc image (*.iso)")
TX(T_FILTRO_ZIP,        "Archivo zip (*.zip)", "Zip file (*.zip)")

/* ---- Herramientas ---- */
TX(T_SEC_CARPETAS,      "Carpetas", "Folders")
TX(T_ABRIR_PARTIDAS,    "Abrir carpeta de partidas", "Open saves folder")
TX(T_ABRIR_JUEGO,       "Abrir carpeta del juego", "Open game folder")
TX(T_SEC_COPIAS,        "Copias de las partidas", "Save backups")
TX(T_COPIAR,            "Hacer copia ahora", "Back up now")
TX(T_RESTAURAR,         "Restaurar la copia elegida", "Restore the chosen backup")
TX(T_COPIAS_AYUDA,      "Las copias van a %ls, con la fecha. Restaurar primero hace otra copia de lo que hay: nunca se borra nada.",
                        "Backups go to %ls, dated. Restoring first backs up what is there: nothing is ever deleted.")
TX(T_COPIA_OK,          "Copia hecha: %ls (%d archivos).", "Backup done: %ls (%d files).")
TX(T_COPIA_ERR,         "No pude hacer la copia (error %lu).", "Could not make the backup (error %lu).")
TX(T_COPIA_NADA,        "No hay partidas que copiar.", "There are no saves to back up.")
TX(T_SIN_COPIAS,        "(no hay copias)", "(no backups)")
TX(T_PREG_RESTAURAR,    "¿Restaurar la copia «%ls»? Antes se copian las partidas actuales.", "Restore the backup “%ls”? The current saves are backed up first.")
TX(T_RESTAURADO,        "Restaurada «%ls». Las partidas que había están en «%ls».", "Restored “%ls”. The previous saves are in “%ls”.")
TX(T_SEC_MEDIR,         "Medir", "Measure")
TX(T_MEDIR_TODO,        "Medir todo", "Measure everything")
TX(T_MEDIR_AYUDA,       "Igual que «MEDIR TODO - Obi-Wan 6.bat»: juega con todas las medidas encendidas y, al cerrar, copia el log, los tiempos de cada imagen, las capturas y las voces a logs\\medida-<fecha> junto al lanzador.",
                        "Same as “MEDIR TODO - Obi-Wan 6.bat”: plays with every measurement on and, when closed, copies the log, frame times, captures and voices to logs\\medida-<date> next to the launcher.")
TX(T_SEC_DIAG,          "Diagnóstico", "Diagnostics")
TX(T_EXPORTAR_DIAG,     "Exportar diagnóstico (.zip)…", "Export diagnostics (.zip)…")
TX(T_DIAG_AYUDA,        "Junta el estado del juego, obiwan.ini y los 3 últimos logs en un zip para pedir ayuda. Quita tu nombre de usuario y el nombre del equipo.",
                        "Puts the game status, obiwan.ini and the last 3 logs in a zip to ask for help. Your user name and computer name are removed.")
TX(T_DIAG_OK,           "Diagnóstico guardado en %ls (%d archivos).", "Diagnostics saved to %ls (%d files).")
TX(T_DIAG_ERR,          "No pude escribir el zip (error %lu).", "Could not write the zip (error %lu).")
TX(T_SEC_VERIF,         "Archivos del juego", "Game files")
TX(T_VERIFICAR,         "Verificar archivos del juego", "Verify game files")
TX(T_VERIF_AYUDA,       "Calcula la firma de cada archivo y la compara con EXTRACCION.json. Tarda unos minutos y no cambia nada.",
                        "Computes the signature of every file and compares it with EXTRACCION.json. It takes a few minutes and changes nothing.")
TX(T_VERIF_OK,          "Todo bien: %d archivos comprobados.", "All good: %d files checked.")
TX(T_VERIF_MAL,         "%d cambiados, %d que faltan y %d de más (lista en %ls).", "%d changed, %d missing and %d extra (list in %ls).")
TX(T_VERIF_PROGRESO,    "Comprobando %d de %d…", "Checking %d of %d…")
TX(T_VERIF_SIN,         "No hay EXTRACCION.json en la carpeta del juego: no hay con qué comparar.", "There is no EXTRACCION.json in the game folder: nothing to compare with.")
TX(T_BORRAR_CACHE,      "Borrar caché de shaders", "Delete shader cache")
TX(T_CACHE_AYUDA,       "Manda la carpeta shadercache a la papelera (se puede recuperar). El juego la rehace; el primer arranque va algo más a tirones.",
                        "Sends the shadercache folder to the recycle bin (it can be recovered). The game rebuilds it; the first start stutters a bit more.")
TX(T_CACHE_OK,          "Caché de shaders enviada a la papelera.", "Shader cache sent to the recycle bin.")
TX(T_CACHE_NO,          "No hay caché de shaders.", "There is no shader cache.")
TX(T_CACHE_ERR,         "No pude mover la caché a la papelera (error %d).", "Could not move the cache to the recycle bin (error %d).")
TX(T_SEC_PERFILES,      "Perfiles", "Profiles")
TX(T_PERFIL_NOMBRE,     "Nombre del perfil", "Profile name")
TX(T_GUARDAR_PERFIL,    "Guardar como perfil", "Save as profile")
TX(T_CARGAR_PERFIL,     "Cargar perfil", "Load profile")
TX(T_PERFIL_AYUDA,      "Un perfil es una copia de estos ajustes en perfiles\\<nombre>.ini. «Jugar Obi-Wan 6.exe --perfil <nombre>» juega con él.",
                        "A profile is a copy of these settings in perfiles\\<name>.ini. “Jugar Obi-Wan 6.exe --perfil <name>” plays with it.")
TX(T_PERFIL_OK,         "Perfil guardado en %ls.", "Profile saved to %ls.")
TX(T_PERFIL_CARGADO,    "Perfil «%ls» cargado: pulsa Guardar para usarlo al jugar.", "Profile “%ls” loaded: press Save to use it when playing.")
TX(T_PERFIL_MAL,        "Nombre no válido: solo letras, números, punto, guion y guion bajo.", "Invalid name: only letters, digits, dot, dash and underscore.")
TX(T_SIN_PERFILES,      "(no hay perfiles)", "(no profiles)")
TX(T_RESTABLECER,       "Restablecer todo", "Reset everything")
TX(T_PREG_RESTABLECER,  "¿Volver todos los ajustes a los de por defecto? Antes se guarda una copia del obiwan.ini actual (obiwan.ini.bak-<fecha>).", "Return every setting to its default? A copy of the current obiwan.ini is kept first (obiwan.ini.bak-<date>).")
TX(T_RESTABLECIDO,      "Ajustes por defecto guardados. El anterior está en %ls.", "Defaults saved. The previous file is in %ls.")
TX(T_SEC_MANTENIMIENTO, "Mantenimiento", "Maintenance")
TX(T_CIERRA_JUEGO,      "Cierra el juego antes de hacer esto.", "Close the game before doing this.")

/* ---- Desarrollador ---- */
TX(T_DEV_AVISO,         "Estos interruptores son para medir y comparar. Pueden hacer que el juego vaya peor o no arranque. ¿Activar el modo desarrollador?",
                        "These switches are for measuring and comparing. They can make the game run worse or not start. Turn developer mode on?")
TX(T_DEV_INACTIVO,      "Con el modo apagado, el lanzador ignora toda la sección [Developer]: los arreglos siguen encendidos y las medidas apagadas.",
                        "With the mode off, the launcher ignores the whole [Developer] section: fixes stay on and measurements off.")
TX(T_SEC_ARREGLOS,      "Arreglos (encendidos por defecto)", "Fixes (on by default)")
TX(T_SEC_MEDIDAS,       "Medidas (apagadas por defecto)", "Measurements (off by default)")
TX(T_SEC_OTROS,         "Otros", "Other")
TX(T_VER_ENTORNO,       "Ver qué pondría el lanzador", "Show what the launcher would set")
TX(T_VER_ENTORNO_AYUDA, "Ejecuta «Jugar Obi-Wan 6.exe --en-seco» con estos ajustes (aunque no estén guardados): enseña las variables y la orden sin arrancar el juego.",
                        "Runs “Jugar Obi-Wan 6.exe --en-seco” with these settings (even if not saved): shows the variables and the command without starting the game.")

/* ---- Créditos ---- */
TX(T_CREDITOS_TEXTO,    "Port de PC de Star Wars: Obi-Wan (Xbox, 2001) por recompilación estática del ejecutable original. Necesitas tu propia copia del juego: aquí no hay datos del juego.\r\n\r\n"
                        "Piezas y licencias (el texto completo de cada una está debajo):\r\n"
                        "· obiwan-recomp, el proyecto del port: GPL-3.0.\r\n"
                        "· xboxrecomp (runtime: kernel, vídeo, audio y entrada): MIT.\r\n"
                        "· Partes del render tomadas de Two Towers Recompiled (gpu_tt): MIT, con sus créditos en cada archivo.\r\n"
                        "· Partes de xemu (sonido MCPX APU y registros NV2A): LGPL-2.1 o posterior. Autores en NOTICE: espes, Jannik Vogel y Matt Borgerson.\r\n"
                        "· SDL2 (mandos): licencia zlib.\r\n"
                        "· obiwan.ini, el lanzador 6 y este configurador: parte de este port (Claude, 2026).",
                        "PC port of Star Wars: Obi-Wan (Xbox, 2001) by static recompilation of the original executable. You need your own copy of the game: no game data is included.\r\n\r\n"
                        "Parts and licences (each full text is below):\r\n"
                        "· obiwan-recomp, the port project: GPL-3.0.\r\n"
                        "· xboxrecomp (runtime: kernel, video, audio and input): MIT.\r\n"
                        "· Render parts taken from Two Towers Recompiled (gpu_tt): MIT, credited in each file.\r\n"
                        "· Parts of xemu (MCPX APU sound and NV2A registers): LGPL-2.1 or later. Authors in NOTICE: espes, Jannik Vogel and Matt Borgerson.\r\n"
                        "· SDL2 (controllers): zlib licence.\r\n"
                        "· obiwan.ini, launcher 6 and this configurator: part of this port (Claude, 2026).")
TX(T_MARCAS,            "Star Wars, Obi-Wan y los nombres relacionados son marcas de sus dueños (Lucasfilm Ltd.). Este port no está hecho, afiliado ni respaldado por ellos.",
                        "Star Wars, Obi-Wan and related names are trademarks of their owners (Lucasfilm Ltd.). This port is not made, affiliated with or endorsed by them.")
TX(T_LICENCIA,          "Licencia", "Licence")
#endif
