# Estado de UBF — 4 de octubre de 2026

Este inventario separa código implementado, comprobaciones realizadas y trabajo aún abierto. La compilación del juego no demuestra por sí sola que la experiencia funcione en pantalla.

## Implementado en el prototipo local

- Catálogo de 13 definiciones en tiempo de ejecución: Wizz, Akira, Eilene, Nozomidol, EmikoAi, Maybkchan, Helen Creth, Gizz, Iria Voss, Kael Serein, Nara Quill, Riven Sol y Tavi Orun. La selección se guarda en el perfil local. La fase de draft impide repetir personaje entre compañeros y la IA toma personajes libres de su propio equipo.
- Nombres de habilidades Q/E/F por personaje y mecánicas distintas de área, línea, pulso, atracción, empuje, embestida y contraataque. Las pasivas configuradas afectan gauge, daño, dash, marcas, áreas, cargados aéreos o duración de contraataque.
- Enhanced Input con acciones separadas para habilidad primaria, secundaria y Ultimate. El menú de Opciones captura teclado y botones de ratón, conserva las asignaciones y rechaza duplicados entre habilidades o teclas ya reservadas para movimiento, ataque, salto, dash y menú.
- HUD con personaje, barras Q/E/F, cooldowns y nombres de las teclas remapeadas.
- IA con perfiles por personaje, demora de reacción imperfecta, frecuencia y prioridad de habilidades variables, selección de objetivo ponderada y retirada con poca salud. Ahora el perfil `DodgeReaction` puede provocar un dash evasivo ante una carga o un ataque recién ejecutado; `RecoveryBehavior` aumenta la cautela al quedar con poca salud. El bot invoca acciones de combate del mismo personaje, sin simular teclas. Fácil, Normal y Difícil cambian la toma de decisiones; no multiplican el daño.
- Selector de Arena/Golem, formato 1v1 a 5v5 y llenado de plazas con bots. El draft dibuja maniquíes locales enfrentados y ajusta su tamaño/espaciado al formato; Team A se muestra rojo y Team B azul. El Golem Dorado es atacable y el último golpe entrega la espada; el servidor solo permite al portador dañar al Master Golem rival. Los Master/Golem dorado tienen una silueta provisional de primitivas de motor, aproximadamente 4.6 m de altura. En Golem War se gana destruyendo el Master rival o eliminando al equipo contrario; no se puntúa por sostener la espada.
- Menú de pausa dibujado como botones clicables con acciones de retomar, rendirse (bloqueada hasta ronda 3) y abandonar sala. Las RPC del servidor validan la rendición, evitan rellenar con bots una plaza durante combate y devuelven al menú tras los resultados; una salida que deja equipos humanos desbalanceados fuerza empate.
- Flujo de entrada actualizado: al iniciar desde la sala local aparece un draft de hasta 30 segundos con selección de personaje, roster de ambos equipos y resumen de Q, E, Ultimate y pasiva. Si el jugador confirma antes del límite, una cuenta atrás de cinco segundos abre la segunda pantalla de carga. Los bots eligen personajes únicos dentro de su equipo y se marcan listos automáticamente.
- Tras abrir el mapa, el juego mantiene a los combatientes bloqueados hasta que todos los jugadores humanos conectados marquen «listo» con Enter o A en el mando; los bots ya cuentan como listos. Después hay una cuenta regresiva animada de tres segundos; los combatientes se desbloquean al entrar en `InRound`. La misma barrera se vuelve a usar al preparar cada ronda.
- La voz de presentación usa el `SoundWave` normalizado `introdution_ubf_normalized`; durante el conteo de la primera ronda también suena `introdution_ubf.wav` a volumen completo y no se repite en rondas posteriores. El canal de esta locución es independiente del anuncio de combate: `round_introduction_1.wma` empieza al terminar los tres segundos, mientras se muestra «¡LA BATALLA HA COMENZADO!» y se desbloquea el movimiento. Al acabar ese anuncio, continúa el soundtrack de batalla al 60 %.
- Los avisos del Golem se replican como eventos de partida: todos oyen el audio aliado o enemigo correcto cuando cae el Golem Dorado; solo los defensores oyen la alerta al primer impacto válido del portador de la espada y al alcanzar 50 % y 10 % de vida su Master. Los impactos de enemigos sin espada no causan daño ni generan avisos. Las voces y alertas nuevas usan WMA para WmfMedia y nivel normalizado; los temas de batalla y los audios de victoria/derrota conservan el 60 %.
- Juego y habilidades siguen usando `AUBFCombatCharacter` y la misma ruta de daño para jugador y bots.
- Perfil local persistente con oro, Puntos de Evento, tickets de gacha, inventario y códigos ya canjeados. Los cinco códigos indicados entregan las recompensas configuradas; `MAKEONLYFAI23` genera un anillo y un collar aleatorios. El guardado es transaccional: si falla, revierte las recompensas y no marca el código como usado.
- La página Códigos muestra los códigos disponibles. Gacha tiene ahora una página propia con el saldo animado de tickets; acumular funciona y gastar permanece deshabilitado. La tienda y las demás páginas tienen títulos alineados con su contenido.
- Al canjear recompensas, un aviso animado muestra lo recibido y los contadores de oro, evento y tickets suben hasta los saldos guardados.

## Verificación realizada

- `UBF Win64 Development` y `UBF Win64 Shipping` compilaron correctamente después de integrar esquiva de bots; ese artefacto corresponde a una versión anterior a la alineación, menú de rendición y regla de último golpe añadidos en esta actualización.
- El 4 de octubre de 2026, `UBF Win64 Development` volvió a compilar correctamente en `D:\proyectos\UBF-working` después de integrar draft, selección única de personajes de bots y la barrera de jugadores listos. El ejecutable recién generado es `D:\proyectos\UBF-working\Binaries\Win64\UBF.exe`. Esta compilación no valida aún el comportamiento dentro de PIE.
- El 5 de octubre de 2026, `UBF Win64 Development` y `UBF Win64 Shipping` compilaron correctamente con el conteo de tres segundos, avisos replicados del Golem y la separación de audio para que `round_introduction_1.wma` no espere al fin de la locución inicial. El WAV normalizado midió `-14.46 LUFS` y `-1.00 dBTP`; Unreal importó una copia `SoundWave` de 9.22 s. El nuevo `BuildCookRun` completó cook, stage, IoStore y archive.
- El Shipping archivado se ejecutó una vez con `NullRHI`, sin sonido y cierre por línea de comandos; salió con código 0. La salida solo registró estadísticas de memoria al cerrar, así que esto comprueba el arranque/cierre del proceso y no sustituye la prueba del mapa, del HUD ni de los audios.
- La compilación terminó sin errores. Quedaron avisos de API deprecada de `APawn::GetMovementBase` en el encabezado de Unreal Engine 5.8 y un aviso de que la toolchain de Visual Studio instalada es posterior a la recomendada.
- La sesión original del editor permanece abierta. Live Coding informó que no detectó cambios y PIE siguió ejecutando su DLL anterior; por ello, esa sesión no certifica el nuevo canal de audio ni el draft. El MCP solo capturó el visor del editor, no una imagen verificable de la ventana de juego.
- `UBFEditor` compiló correctamente en `D:\proyectos\UBF-working`, un clon separado, sin cerrar el editor original. También se lanzó `Binaries/Win64/UBF.exe` Development desde ese clon y se verificó el registro de la secuencia de presentación anterior a esta conversión: `intro.mp4` confirmó reproducción; `musicintro` comenzó al segundo 2.000 desde offset 0 a 60 %; WmfMedia notificó 13.200 s y se conservó el corte configurado de 13.240 s; `videobackground.mp4` y la antigua voz MP3 comenzaron con offset 0.208 s; apareció la fase interactiva y, al terminar `musicintro`, empezó `musicintrobucle` a 60 %. La verificación fue por registro de ejecución; todavía no se comprobó una partida mediante interacción manual.
- El primer cocinado detectó una entrada obsoleta `GameFeatureData` en `DefaultGame.ini`; UBF no usa ni habilita ese plugin. Se retiró la entrada tanto del clon como del proyecto abierto. El siguiente `BuildCookRun` terminó correctamente con cook, stage, IoStore y archive.
- Candidata local `publish/UBF-v1.0.6-beta.zip`: 551,142,608 bytes, SHA-256 `6b31c8615c568b1eab9db76f57330b1f58ba864321c09c5d8453421fb3be90b7`. El ZIP contiene 70 entradas; los 38 archivos que comprueba el manifiesto se encontraron y sus hashes se calcularon desde el archivo. `publish/manifest-v1.0.6-beta.json` es solo candidato. El manifiesto oficial todavía apunta a `1.0.5-beta` y no se creó ni publicó una nueva Release.
- El ejecutable Shipping recién cocinado se abrió en modo smoke con `-nullrhi -nosound` y salió con código 0. Esto verifica inicio/cierre del proceso, no la imagen de juego ni audio audible.
- Nueva candidata local: `publish/UBF-v1.0.7-beta-audio-sync.zip` (569,539,017 bytes, SHA-256 `1e00471d76673ad2ba6ba6dfe44b9c884a776811ce4475f36a0af1133e686a28`), con `publish/manifest-v1.0.7-beta-audio-sync.json`; el ZIP contiene 55 archivos y los 51 paths del manifiesto coinciden en tamaño y SHA-256. Es un paquete del juego, no del launcher. El manifiesto oficial sigue en `1.0.5-beta`; no se publicó una Release porque el PIE disponible usa la DLL anterior.

## Pendiente antes de considerar completa la expansión

| Área | Pendiente |
|---|---|
| Códigos/cuenta | El uso único se conserva en el SaveGame del perfil local; todavía no existe un servicio de cuenta/servidor que impida eludirlo borrando o reemplazando ese archivo. No presentarlo como una protección online. |
| Assets | Los 13 luchadores siguen usando el maniquí y animaciones compartidas de UAL; falta una identidad visual/animación individual, retratos, VFX, SFX y revisión visual dentro del juego. La licencia CC0 de UAL1/UAL2 está registrada en `Docs/Characters/ASSETS_Y_LICENCIAS.md`. Los conceptos VTuber continúan siendo provisionales si no hay fuentes primarias suficientes. |
| Habilidades | Hay geometrías y efectos de prototipo, pero no todos los campos de Ability Data se consumen: cast time, knockdown, cámara y VFX/SFX/animación por habilidad. La pasiva no sustituye esos sistemas. |
| IA | No hay StateTree/Behavior Tree ni estados completos de protección/persecución. Las prioridades Golem y el scoring básico están conectados, pero la navegación táctica todavía es limitada. |
| Golem Mode | Falta prueba PIE del último golpe, espada, avisos de ataque al primer contacto/50 %/10 %, restricciones de daño al Master, respawn del Golem Dorado, condición de victoria y bots. Los modelos hechos con primitivas son provisionales y necesitan revisión visual y de escala. |
| Salas | La sala y el draft siguen siendo locales y usan bots; no hay sesiones online ni lobby remoto. El servidor replica el estado de preparación, además de validar rendición/abandono y el roster tras una desconexión. Falta probar en PIE/servidor-cliente Enter/A, conteo de listos, empate por desconexión, rendición en ronda 3 y las cinco distribuciones visuales del draft. |
| Controles | El remapeo persistente funciona para Q/E/F en el prototipo; falta comprobarlo dentro de PIE, con cambio de mapa y reinicio. |
| Cierre | Faltan pruebas de salida, muerte, reinicio, cambio de mapa y timers/delegates/actores temporales para todas las rutas, en especial Golem Mode. |
| Distribución | El ZIP candidato `1.0.7-beta` se cocinó y verificó localmente, pero aún no pasó una partida completa ni el flujo Launcher → juego. El `manifest.json` oficial conserva `1.0.5-beta`; las versiones públicas del Launcher y Setup siguen independientes. |
| Release | El PIE de la sesión abierta no usa la última DLL y no fue posible verificar la ventana de juego desde el MCP. Falta probar con el módulo actualizado el draft de 30 segundos, la partida, los bots, la barrera de listos y el Golem; el ZIP nuevo es candidato local, todavía no Release. La Release del Launcher 1.0.18 también espera validación interactiva y publicación de su paquete independiente. |

## Referencias de diseño documentadas

Los conceptos y kits están en `Docs/Characters/`. No se afirma que los avatares públicos se hayan confirmado: la información insuficiente está indicada dentro de cada ficha. Guías iniciales de IA, controles y salas están en `Docs/AI/`, `Docs/Input/` y `Docs/Rooms/`.

## Investigación inicial de referencias

Consulta pública realizada el 4 de octubre de 2026. La evidencia disponible orienta solo rasgos de contenido/autodescripción; no basta para definir o copiar avatares:

- [Nozomidol](https://streamlape.com/en/creator/nozomidol): ficha secundaria la describe como artista/VTuber bilingüe con temas de sirena, canto y voz; el avatar no se pudo verificar en una página primaria legible.
- [EmikoAi](https://mobile.twstalker.com/EmikoAiVT): una copia indexada de su bio lo presenta como comediante VTuber y menciona varios idiomas, juegos FPS/Strinova; el About de Twitch no fue indexable.
- [Maybkchan](https://twitchtracker.com/maybkchan_): ficha de TwitchTracker refleja una autodescripción de juego relajado en español; no aporta evidencia suficiente del diseño del avatar.
- [Helen Creth](https://vgen.co/HCresth): la página de @HCresth indica contenido en español/inglés y enlaza redes; [Linktree](https://linktr.ee/helencreth) incluye Twitch. No se pudo verificar avatar ni biografía accesible del canal.
- [Gizz / GizzMurdock](https://bsky.app/profile/gizzmurdock.bsky.social): su bio pública muestra Giselle y contenido de juegos de HoYoverse/Kuro Games; enlaza Twitch y no confirma el avatar. Es una sola persona/personaje.

Las fichas individuales registran estas fuentes y separan explícitamente hechos, autodescripciones y propuestas provisionales.

## Siguiente secuencia de validación

1. Cargar el módulo actualizado en Unreal Editor y probar Arena 1v1/2v2/3v3/4v4/5v5, composición del draft, selección única, animación/posición de los maniquíes, bots y Enter/A después de cada carga.
2. Probar cuenta regresiva inmóvil de tres segundos y confirmar que Round 1 inicia la voz solo una vez; al aparecer «¡LA BATALLA HA COMENZADO!» deben activarse movimiento y `round_introduction_1`.
3. Probar Golem Mode: último golpe al Golem Dorado, audio/texto aliado y enemigo, alertas locales al primer impacto y a 50 %/10 %, ataques sin espada rechazados sin avisos, muerte del portador y respawn del Golem neutral, condición de victoria y reinicio.
4. Probar el menú de pausa: botones de ratón, rendición deshabilitada en rondas 1–2 y aceptada en ronda 3, abandono, empate por desconexión y regreso al menú.
5. Reiniciar Unreal Editor para cargar el módulo actualizado, compilar `UBFEditor` y ejecutar las pruebas PIE de las rondas, sala, bots y avisos de Golem indicadas arriba.
6. Probar el candidato Shipping local `1.0.7-beta` desde el launcher instalado; si se corrige algo, reconstruirlo y verificar otra vez el ZIP y su manifest.
7. Tras aprobar esas pruebas, actualizar el manifest oficial desde `1.0.5-beta`, sincronizar el repositorio del juego y publicar el paquete UBF por separado de las versiones del Launcher y Setup.
