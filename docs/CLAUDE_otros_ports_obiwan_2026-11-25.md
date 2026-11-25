# ¿Alguien más ha portado Star Wars: Obi-Wan? · 25 de noviembre de 2026

[H] = comprobado leyendo la página o el código público; [U] = conocimiento general; [Hip] = hipótesis.
No se ha descargado ni ejecutado nada de terceros: solo se ha leído código y páginas en la web.

## Resumen

| Proyecto | Qué es | Fecha | Estado real |
|---|---|---|---|
| [Star Wars: Obi Comp](https://github.com/onedeck-launcher/star-wars-obi-comp) (antes [goodthanksdev/star-wars-obi-comp](https://github.com/goodthanksdev/star-wars-obi-comp)) | Recompilación estática a PC con [Xbox XBE Recomp](https://github.com/TGP482/XboxXBERecomp) (fork [goodthanksdev/XboxXBE-Recomp](https://github.com/goodthanksdev/XboxXBE-Recomp)), Direct3D 12 | v0.1.0 17-11-2026, [v0.2.0](https://github.com/onedeck-launcher/star-wars-obi-comp/releases/tag/v0.2.0) 22-11-2026 | [H] **Alfa jugable**. El juego se compila en el PC del jugador a partir de su XBE. v0.2.0: pantalla ancha real, "frame rate más suave, fallo de iluminación arreglado", trucos y mods. 4 estrellas, 40 descargas de v0.2.0. Fallo conocido: algunos ajustes de sonido 3D. |
| [soltros/obiwan-recomp](https://github.com/soltros/obiwan-recomp) + [xbox-linux](https://github.com/soltros/xbox-linux) | Recompilación estática para Linux, basada en xboxrecomp | 2026 | [H] **A medias**: llega al menú y dibuja el logo, el texto del menú sale mal y no se juega. GPL-3.0. |
| austinbaccus/re-obiwan | Ingeniería inversa con Ghidra | — | [H] El enlace da 404 (repositorio borrado o privado). |
| xemu ([ficha](https://xemu.app/titles/4156000f/)) | Emulador | última prueba 19-06-2025 | [H] "Starts": texto del menú corrupto y cuelgues en niveles grandes; incidencia de cuelgue en el pantano (#1673). No sirve como referencia de 240 fps. |
| Cxbx-Reloaded ([blog de 2019](https://www.kyleniewiada.org/blog/2019/08/fixing-star-wars-obi-wan/)) | Emulador | 2019 | [U] Se puede jugar con fallos gráficos y cuelgues. |

Fuera de esto no hay ports oficiales ([Kotaku 2024](https://kotaku.com/star-wars-obi-wan-kenobi-xbox-game-remaster-port-remake-1851571051)).
No se encontraron vídeos ni hilos de Reddit sobre Obi Comp.

## Diferencia de arquitectura con nuestro port

- [H] **Nuestro port** (base Two Towers / xboxrecomp) recompila también la biblioteca Direct3D 8 del juego. Esa biblioteca
  escribe el pushbuffer NV2A, y un hilo nuestro lo lee y lo ejecuta sobre D3D11 (LLE de la GPU).
- [H] **Obi Comp** sustituye las funciones Direct3D 8 del XDK 3911 por funciones propias (HLE): `D3DDevice_DrawIndexedVertices`,
  `SetTexture`, `SetRenderState_*`, etc., con direcciones en su `config/game.toml`. No hay pushbuffer.
  `D3D_BlockOnTime` (0x3705B0) devuelve 0 al momento y `CDevice_KickOff` (0x370460) no hace nada (`runtime/xdk/d3d8.cpp`, l. 548 y 571).
  Son las mismas direcciones que vemos en nuestros perfiles (`sub_003705B0`, `sub_00370460`).

## Lo que podemos aprender

1. **Las esperas son el cuello de rendimiento, y tienen arreglo.** [H] Commit del 17-11-2026 en el toolkit: "Queued draws carry a copy
   of the vertices they read ... (Obi-Wan: ~1500 waits, ~21 ms a frame, 32-45 fps -> 60)". El juego rellena buffers de vértices
   unas 1500 veces por frame, y antes de cada relleno espera a que la GPU acabe con ellos. Ellos copian los vértices al poner el draw
   en cola, así que el juego ya no espera. [Hip] Es el mismo patrón que nuestras ~1370 esperas por frame en combate:
   `sub_003708B0` (BlockOnResource) → `sub_003705B0` (BlockOnTime). La versión LLE de su idea sería capturar los vértices
   cuando la fence correspondiente se da por cumplida, o hacer el recorrido del pushbuffer en el hilo del juego.
2. **Sombra del jugador.** [H] Commit del 17-11-2026: "Shadow maps from F16 depth buffers: 0-511.9375 range ..., border depth reversed".
   Su sombra salía como una mancha clara alrededor del jugador, y en algunos niveles se estiraba a lo lejos. Había dos fallos:
   la profundidad F16 usaba el rango de 24 bits, y el texel del borde (FFFFFFFF = lejos) se comparaba como cerca tras invertir la profundidad.
   [Hip] Hay que comprobar si nuestro `sdepth`/`spcf` trata igual los buffers de profundidad en coma flotante de 16 bits.
3. **Recurso basura en AddRef.** [H] Su `hooks.cpp` protege `sub_0036CE20` (AddRef usado por `D3D_RecordStateBlock` 0x36763C)
   frente a punteros ≥ 0xF0000000. En su port, render states mal numerados dejaban un recurso 0xFFAAAAAA, y eso colgaba el juego.
   [Hip] Nuestras excepciones en SA_05 y L05_7 (escritura en 0x84010000) pueden ser de la misma familia.
4. **Pantalla ancha sin recortes.** [H] Enganchan `sub_0002F7A0` (campo de visión de la cámara) y ensanchan el FOV horizontal a 16:9.
   Así el propio juego dibuja y descarta la geometría con el ángulo ancho. Es una mejora no-consola fácil para el futuro.
5. **Desbloqueos.** [H] `sub_00096170` devuelve el nivel más alto desbloqueado; los niveles van de 0 a 26 y L05_7 es el último.

Licencias: el toolkit Xbox XBE Recomp es MIT. Si se copia código, hay que poner el crédito, igual que con Two Towers.
