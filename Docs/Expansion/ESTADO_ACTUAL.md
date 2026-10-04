# Estado de UBF — 4 de octubre de 2026

Este inventario separa código implementado, comprobaciones realizadas y trabajo aún abierto. La compilación del juego no demuestra por sí sola que la experiencia funcione en pantalla.

## Implementado en el prototipo local

- Catálogo de 13 definiciones en tiempo de ejecución: Wizz, Akira, Eilene, Nozomidol, EmikoAi, Maybkchan, Helen Creth, Gizz, Iria Voss, Kael Serein, Nara Quill, Riven Sol y Tavi Orun. La selección se guarda en el perfil local y los personajes repetidos siguen permitidos.
- Nombres de habilidades Q/E/F por personaje y mecánicas distintas de área, línea, pulso, atracción, empuje, embestida y contraataque. Las pasivas configuradas afectan gauge, daño, dash, marcas, áreas, cargados aéreos o duración de contraataque.
- Enhanced Input con acciones separadas para habilidad primaria, secundaria y Ultimate. El menú de Opciones captura teclado y botones de ratón, conserva las asignaciones y rechaza duplicados entre habilidades o teclas ya reservadas para movimiento, ataque, salto, dash y menú.
- HUD con personaje, barras Q/E/F, cooldowns y nombres de las teclas remapeadas.
- IA con perfiles por personaje, demora de reacción imperfecta, frecuencia y prioridad de habilidades variables, selección de objetivo ponderada y retirada con poca salud. Ahora el perfil `DodgeReaction` puede provocar un dash evasivo ante una carga o un ataque recién ejecutado; `RecoveryBehavior` aumenta la cautela al quedar con poca salud. El bot invoca acciones de combate del mismo personaje, sin simular teclas. Fácil, Normal y Difícil cambian la toma de decisiones; no multiplican el daño.
- Selector de Arena/Golem, formato 1v1/2v2/3v3 y llenado de plazas con bots. Golem Mode incluye dos Master Golem atacables, orbe central/dropeable, portador, puntos por mantenerlo 25 segundos y victoria por tres puntos o al destruir el Master enemigo.
- Juego y habilidades siguen usando `AUBFCombatCharacter` y la misma ruta de daño para jugador y bots.
- Perfil local persistente con oro, Puntos de Evento, tickets de gacha, inventario y códigos ya canjeados. Los cinco códigos indicados entregan las recompensas configuradas; `MAKEONLYFAI23` genera un anillo y un collar aleatorios. El guardado es transaccional: si falla, revierte las recompensas y no marca el código como usado.
- La página Códigos muestra los códigos disponibles. Gacha tiene ahora una página propia con el saldo animado de tickets; acumular funciona y gastar permanece deshabilitado. La tienda y las demás páginas tienen títulos alineados con su contenido.
- Al canjear recompensas, un aviso animado muestra lo recibido y los contadores de oro, evento y tickets suben hasta los saldos guardados.

## Verificación realizada

- `UBF Win64 Development` y `UBF Win64 Shipping` compilaron correctamente después de integrar esquiva de bots; el ejecutable Shipping actual es `Binaries/Win64/UBF-Win64-Shipping.exe`.
- La compilación terminó sin errores. Quedaron avisos de API deprecada de `APawn::GetMovementBase` en el encabezado de Unreal Engine 5.8 y un aviso de que la toolchain de Visual Studio instalada es posterior a la recomendada.
- No se verificó la ejecución visual ni una partida. El editor abierto no se pudo controlar desde esta sesión y el endpoint local de Unreal MCP (`127.0.0.1:8001`) no respondió.
- `UBFEditor` y `BuildCookRun` siguen pendientes: el editor mantiene Live Coding activo. No se cerró ni se intentó eludir ese bloqueo.

## Pendiente antes de considerar completa la expansión

| Área | Pendiente |
|---|---|
| Códigos/cuenta | El uso único se conserva en el SaveGame del perfil local; todavía no existe un servicio de cuenta/servidor que impida eludirlo borrando o reemplazando ese archivo. No presentarlo como una protección online. |
| Assets | Los 13 luchadores siguen usando el maniquí y animaciones compartidas de UAL; falta una identidad visual/animación individual, retratos, VFX, SFX y revisión visual dentro del juego. La licencia CC0 de UAL1/UAL2 está registrada en `Docs/Characters/ASSETS_Y_LICENCIAS.md`. Los conceptos VTuber continúan siendo provisionales si no hay fuentes primarias suficientes. |
| Habilidades | Hay geometrías y efectos de prototipo, pero no todos los campos de Ability Data se consumen: cast time, knockdown, cámara y VFX/SFX/animación por habilidad. La pasiva no sustituye esos sistemas. |
| IA | No hay StateTree/Behavior Tree ni estados completos de protección/persecución. Las prioridades Golem y el scoring básico están conectados, pero la navegación táctica todavía es limitada. |
| Golem Mode | Falta prueba PIE de recogida, caída, puntuación, destrucción del Master, marcador replicado y bots. El Master Golem todavía usa el mannequin ampliado como representación provisional. |
| Salas | Las salas son locales y usan bots. No hay sesiones online/lobby de jugadores remotos ni pruebas de red de 2–6 clientes. |
| Controles | El remapeo persistente funciona para Q/E/F en el prototipo; falta comprobarlo dentro de PIE, con cambio de mapa y reinicio. |
| Cierre | Faltan pruebas de salida, muerte, reinicio, cambio de mapa y timers/delegates/actores temporales para todas las rutas, en especial Golem Mode. |
| Distribución | El candidato del Launcher v1.0.18 está en `main` del repo remoto y el ZIP nuevo existe localmente, pero no se publicó una Release. El código del juego y los UAssets están en `main` como commit `52cfd30`; `manifest.json` sigue apuntando al juego ya publicado v1.0.5-beta. Setup permanece en su repositorio separado. |
| Release | La compilación Shipping no es un paquete cocinado. Faltan compilar el target editor, cocinar/archivar Windows, probar el menú/partida y el flujo Launcher → juego, generar un ZIP y manifest nuevos y después crear la Release del juego. La Release del Launcher también espera una validación interactiva visible. |

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

1. Reiniciar Unreal Editor y probar Arena 1v1/2v2/3v3, llenar con bots, remapeo Q/E/F, personajes repetidos y cambio de mapa.
2. Probar Golem Mode: pickup, eliminación del portador, respawn del orbe, puntos, muerte de cada Master y salida/reinicio.
3. Empaquetar una build Shipping y probarla desde el launcher instalado; validar nombre de usuario, versión, manifest y SHA-256.
4. Cuando Live Coding esté desactivado tras reiniciar Unreal Editor, compilar `UBFEditor`, probar PIE y ejecutar el cook/archivo Shipping en el clon oficial.
5. Revisar `git diff/status`, conservar el manifest v1.0.5 hasta disponer de un ZIP nuevo, sincronizar los repositorios y publicar cada producto por separado solo tras las comprobaciones finales.
