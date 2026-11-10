/* SwanStation Xbox - the texts that differ on Xbox, consulted by tr() before its own table (i18n.c).
 * port-notes/specs/S5-xbox-ui.md section 3. Keys are the REF English literals byte for byte (or new strings used only
 * by SWANUI_XBOX code); English gets `en` (the key when NULL), Portuguese (pt-PT, as i18n.c) gets `pt`, and French,
 * Spanish and Japanese get the English Xbox text. Every text stays inside Latin-1 (ui/text.c packs only that range).
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "swanui_xbox_ui.h"
#include "i18n.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    const char *key; /* the English text tr() is called with: the REF literal, exactly */
    const char *en;  /* the Xbox English text; NULL: the key itself (a new string, only in SWANUI_XBOX code) */
    const char *pt;  /* Portugal Portuguese, as the rest of i18n.c; NULL: the English one */
} XboxText;

static const XboxText XBOX_TEXTS[] = {
    /* ---- controller: touchpad, DualSense, PS5 controller, motion sensor */
    /*  1 settings_screen.c:259 */ {"Touchpad shortcuts", "{Touchpad} shortcuts", "Atalhos com {Touchpad}"},
    /*  2 settings_screen.c:1243 */ {"{Touchpad}: tap = {Select}, hold = menu", "{Touchpad}: tap = PS1 Select, hold = shortcuts",
                                    "{Touchpad}: toque = Select da PS1, manter = atalhos"},
    /*  3 settings_screen.c:255 */ {"Touchpad as a mouse", "Right stick as a mouse", "Analógico direito como rato"},
    /*  4 settings_screen.c:255 */ {"Games that support the PlayStation Mouse (Discworld, Lemmings...): slide a finger on the {Touchpad} to move the pointer; {R2} or {Cross} clicks, {L2} or {Circle} right-clicks.",
        "Games that support the PlayStation Mouse (Discworld, Lemmings...): the right stick moves the pointer; {R2} or {Cross} clicks, {L2} or {Circle} right-clicks.",
        "Jogos compatíveis com o rato PlayStation (Discworld, Lemmings...): o analógico direito move o ponteiro; {R2} ou {Cross} clica, {L2} ou {Circle} faz clique direito."},
    /*  5 settings_screen.c:257 */ {"GunCon and Justifier games: point the controller at the screen to aim (motion sensor), {R2} fires, {L2} reloads, {R3} re-centres. Automatic turns it on in every game that supports a light gun.",
        "GunCon and Justifier games: the right stick moves the crosshair, {R2} fires, {L2} reloads, {R3} re-centres. Automatic turns it on in every game that supports a light gun.",
        "Jogos GunCon e Justifier: o analógico direito move a mira, {R2} dispara, {L2} recarrega, {R3} volta a centrar. Automático liga-o em todos os jogos compatíveis com pistola."},
    /*  6 main.c:2042 */ {"Light gun: point the controller at the screen, {R2} fires, {R3} re-centres",
        "Light gun: the right stick aims, {R2} fires, {R3} re-centres",
        "Pistola de luz: o analógico direito aponta, {R2} dispara, {R3} volta a centrar"},
    /*  7 settings_screen.c:239 */ {"Up to 4 with a multitap, for games that support it: each PS5 controller is a player.",
        "Up to 4 with a multitap, for games that support it: each controller is a player.",
        "Até 4 com um multitap, nos jogos compatíveis: cada comando é um jogador."},
    /*  8 settings_screen.c:247 (group) */ {"DualSense", "Feel and special controllers", "Vibração e comandos especiais"},
    /*  9 settings_screen.c:247 */ {"Classic as the game sends it; Soft is gentler; Punchy makes light hits easier to feel, and can shake the triggers too.",
        "Classic as the game sends it; Soft is gentler; Punchy makes light hits easier to feel.",
        "Clássica como o jogo a envia; Suave é mais leve; Intensa torna os toques leves mais fáceis de sentir."},
    /* 10 settings_screen.c:241 */ {"Choose which controller is player 1, 2, 3 and 4: press {Cross} on each in turn. Kept until SwanStationPS5 closes.",
        "Choose which controller is player 1, 2, 3 and 4: press {Cross} on each in turn. Kept until Tsugaru closes.",
        "Escolha que comando é o jogador 1, 2, 3 e 4: prima {Cross} em cada um, à vez. Mantido até fechar o Tsugaru."},
    /* 11 settings_screen.c:341 (S3 sec. 9.4: up to 512 MiB, off with run-ahead) */
    {"Keeps the last 8 seconds so you can go back ({Touchpad} + {L2}). Uses about 200 MB of memory.",
        "Keeps the last 8 seconds so you can go back ({Touchpad} + {L2}). Uses up to about 500 MB of memory, and is off while Run-ahead is on.",
        "Guarda os últimos 8 segundos para poder voltar ({Touchpad} + {L2}). Usa até cerca de 500 MB de memória e fica desligado com a Antecipação ligada."},

    /* ---- Display and core option helps */
    /* 12 settings_screen.c:177 (S3 shader table) */ {"Gives the picture the look of a CRT TV, a handheld LCD or the analog NTSC signal. The most detailed ones are heavy.",
        "Gives the picture the look of a CRT TV or of the analog NTSC signal, with Tsugaru's shaders.",
        "Dá à imagem o aspeto de uma TV CRT ou do sinal NTSC analógico, com os shaders do Tsugaru."},
    /* 13 settings_screen.c:192 (S3 sec. 12.4 Folders/Textures = <cache>\textures) */
    {"Loads HD texture packs. Over FTP, go to /data/SwanStationPS5/cache/ and create the folder textures if it doesn't exist, then create your game's folder inside it, for example \"SCUS-94900\" (Crash Bandicoot), and put the pack's PNG files there. Needs the GPU renderer. Takes effect with the next game.",
        "Loads HD texture packs. With Device Portal, create Tsugaru\\cache\\textures in the app's LocalState, then a folder named after the game's serial inside it, for example \"SCUS-94900\" (Crash Bandicoot), and put the pack's PNG files there. Needs the GPU renderer. Takes effect with the next game.",
        "Carrega pacotes de texturas HD. Com o Device Portal, crie Tsugaru\\cache\\textures no LocalState da aplicação e, lá dentro, uma pasta com o número de série do jogo, por exemplo \"SCUS-94900\" (Crash Bandicoot), e coloque lá os ficheiros PNG do pacote. Requer o renderizador GPU. Aplica-se no próximo jogo."},
    /* 14 core/swanstation_options.c:459 (row 88) */ {"Replace 'VRAM write textures' with DuckStation-formatted texture packs from the 'swanstation/textures' folder inside the frontend's system directory. This only works with the Vulkan and D3D11 (where available) renderers.",
        "Replaces 'VRAM write textures' with texture packs in Tsugaru's format from Tsugaru\\cache\\textures\\<serial>. Needs the GPU renderer.",
        "Substitui as «texturas de escrita na VRAM» por pacotes de texturas no formato do Tsugaru de Tsugaru\\cache\\textures\\<número de série>. Requer o renderizador GPU."},

    /* ---- Achievements (S4 owns the mapping to DuckStation's notifications) */
    /* 15 settings_screen.c:279 (S4 table: 0 TopRight, 1 and 2 TopCenter) */ {"Banner: top right. Compact: a small line at the top. Big trophy: in the middle, like the PS5's.",
        "Where Tsugaru shows the unlock banner: Banner, top right; Compact and Big trophy, top centre.",
        "Onde o Tsugaru mostra o aviso de desbloqueio: Faixa, em cima à direita; Compacto e Troféu grande, em cima ao centro."},
    /* 16 settings_screen.c:273 */ {"Use Sign in below with your user name and password. Only a token stays on the PS5.",
        "Use Sign in below with your user name and password. Only a token stays on the console.",
        "Use Iniciar sessão abaixo com o nome de utilizador e a palavra-passe. Só um token fica na consola."},
    /* 17 settings_screen.c:107 */ {"Sign in to RetroAchievements with your user name and password, typed on the PS5.",
        "Sign in to RetroAchievements with your user name and password, typed on the console.",
        "Inicie sessão no RetroAchievements com o nome de utilizador e a palavra-passe, escritos na consola."},

    /* ---- Library and System tabs */
    /* 18 settings_screen.c:288 */ {"The look of SwanStationPS5: colours, fonts and the shelf's background.",
        "The look of Tsugaru: colours, fonts and the shelf's background.", "O aspeto do Tsugaru: cores, letras e o fundo da prateleira."},
    /* 19 settings_screen.c:312 */ {"Looks for games added to /data/SwanStationPS5/games or a USB drive.",
        "Looks for games added to Tsugaru\\games or a USB drive.", "Procura jogos adicionados a Tsugaru\\games ou a uma unidade USB."},
    /* 20 settings_screen.c:351 */ {"The language of SwanStationPS5's menus.", "The language of Tsugaru's menus.", "O idioma dos menus do Tsugaru."},
    /* 21 settings_screen.c:345 */ {"Saves the game by itself every few minutes into three auto-save slots (the oldest is replaced). Load one from the SwanStationPS5 menu > Auto-saves.",
        "Saves the game by itself every few minutes into three auto-save slots (the oldest is replaced). Load one from the game menu > Auto-saves.",
        "Grava o jogo sozinho a cada poucos minutos em três ranhuras (a mais antiga é substituída). Carregue uma no menu do jogo > Gravações automáticas."},
    /* 22 settings_screen.c:317 */ {"Starts the console's BIOS with no disc, to manage memory cards. While Memory Card 1 Type (Restart) is set to Libretro, the saves will not show: that card belongs to each game, and with no game there is none.",
        "Starts the console's BIOS with no disc, to manage memory cards. While Memory Card 1 Type (Restart) is set to One per game, the saves will not show: that card belongs to each game, and with no game there is none.",
        "Inicia a BIOS da consola sem disco, para gerir os cartões de memória. Enquanto Tipo do cartão de memória 1 (reiniciar) estiver em Um por jogo, as gravações não aparecem: esse cartão pertence a cada jogo e, sem jogo, não existe."},
    /* 23 settings_screen.c:598 */ {"The console whose BIOS Start BIOS boots: it runs the BIOS chosen above for that region. NTSC-U unless you change it here; it goes back to NTSC-U every time SwanStationPS5 opens.",
        "The console whose BIOS Start BIOS boots: it runs the BIOS chosen above for that region. NTSC-U unless you change it here; it goes back to NTSC-U every time Tsugaru opens.",
        "A consola cuja BIOS o Iniciar BIOS arranca: usa a BIOS escolhida acima para essa região. NTSC-U, a menos que a mude aqui; volta a NTSC-U sempre que o Tsugaru abre."},
    /* 24 core/bios_choices.c:86 (S3 Q3) */ {"OpenBIOS (built into the core)", "OpenBIOS (openbios.bin in the bios folder)", "OpenBIOS (openbios.bin na pasta bios)"},
    /* 25 settings_screen.c:584 */ {"Which BIOS the games of this region run: detect it automatically, the OpenBIOS built into the core, or a file of the BIOS folder. It applies when the next game starts.",
        "Which BIOS the games of this region run: detect it automatically, OpenBIOS (openbios.bin in the bios folder), or a file of the BIOS folder. It applies when the next game starts.",
        "A BIOS que os jogos desta região usam: detetar automaticamente, a OpenBIOS (openbios.bin na pasta bios) ou um ficheiro da pasta de BIOS. Aplica-se quando o próximo jogo arranca."},
    /* 26 core/swanstation_options.c:109 (row 52 label; S3 sec. 4.1 maps it to one card per game) */
    {"Libretro (Default)", "One per game (Default)", "Um por jogo (predefinição)"},
    /* 27 settings_screen.c:317,582 (group caption) */ {"SwanStationPS5", "Tsugaru", "Tsugaru"},
    /* 28 settings_screen.c:323 (S4 sec. 5.3 drive rules) */ {"A folder name to look for games in on drives. It is looked for on /mnt/ext0/, /mnt/ext1/ and /mnt/usb0/ to /mnt/usb7/ at once, as the number changes with the drives plugged in: psx searches /mnt/usb0/psx, /mnt/usb1/psx and so on. Leave it empty to turn it off.",
        "A folder name to look for games in at the root of every USB drive (E:, F:...), besides Tsugaru and Tsugaru\\games: psx searches E:\\psx, F:\\psx and so on. The drive must be NTFS and readable by ALL APPLICATION PACKAGES. Leave it empty to turn it off.",
        "Um nome de pasta onde procurar jogos na raiz de cada unidade USB (E:, F:...), além de Tsugaru e Tsugaru\\games: psx procura em E:\\psx, F:\\psx, etc. A unidade tem de ser NTFS e legível por ALL APPLICATION PACKAGES. Deixe vazio para desligar."},
    /* 29 settings_screen.c:2050 */ {"Type the folder name: games are looked for on /mnt/ext0/, /mnt/ext1/ and /mnt/usb0/ to /mnt/usb7/",
        "Type the folder name: games are looked for in it at the root of every USB drive (E:\\, F:\\...)",
        "Escreva o nome da pasta: os jogos são procurados nela na raiz de cada unidade USB (E:\\, F:\\...)"},

    /* ---- shelf, toasts, paths */
    /* 30 main.c:2323 */ {"Screenshot saved in /data/SwanStationPS5/screenshots", "Screenshot saved in Tsugaru\\screenshots",
        "Captura guardada em Tsugaru\\screenshots"},
    /* 31 coverflow.c:1903 */ {"Hold to exit SwanStationPS5...", "Hold {Circle} to close Tsugaru...", "Mantenha {Circle} premido para fechar o Tsugaru..."},
    /* 32 coverflow.c:2310 */ {"Keep holding to close SwanStationPS5...", "Keep holding to close Tsugaru...", "Mantenha premido para fechar o Tsugaru..."},
    /* 33 coverflow.c:973 */ {"SwanStationPS5 can't open /data/SwanStationPS5", "Tsugaru can't write to its data folder",
        "O Tsugaru não consegue escrever na sua pasta de dados"},
    /* 34 coverflow.c:1184 */ {"Can't list the covers folder: unlock /data in Settings, System", "Can't list the covers folder (Tsugaru\\covers)",
        "Não é possível listar a pasta das capas (Tsugaru\\covers)"},
    /* 35 coverflow.c:1185 */ {"No images yet: put .png or .jpg files in /data/SwanStationPS5/covers/",
        "No images yet: put .png or .jpg files in Tsugaru\\covers (Device Portal) or in the game's folder",
        "Ainda sem imagens: coloque ficheiros .png ou .jpg em Tsugaru\\covers (Device Portal) ou na pasta do jogo"},
    /* 36 coverflow.c:1216 */ {"Images from /data/SwanStationPS5/covers/ and the game's folder. Named like the game, they are used without picking.",
        "Images from Tsugaru\\covers and the game's folder. Named like the game, they are used without picking.",
        "Imagens de Tsugaru\\covers e da pasta do jogo. Com o nome do jogo, são usadas sem escolher."},
    /* 37 memcards_screen.c:351 */ {"One card per game. Import from /data/SwanStationPS5/import.",
        "One card per game. Import from Tsugaru\\import (copy the files there with Device Portal).",
        "Um cartão por jogo. Importe de Tsugaru\\import (copie lá os ficheiros com o Device Portal)."},
    /* 38 memcards_screen.c:433 (arg: app.paths.root) */ {"Put .mcr, .mcd, .srm or .gme files in %s/import",
        "Put .mcr, .mcd, .srm or .gme files in Tsugaru\\import%.0s with Device Portal",
        "Coloque ficheiros .mcr, .mcd, .srm ou .gme em Tsugaru\\import%.0s com o Device Portal"},
    /* 39 memcards_screen.c:248 (arg: the exported file's full path) */ {"Exported to %s", "Exported to Tsugaru\\memcards-export%.0s",
        "Exportado para Tsugaru\\memcards-export%.0s"},
    /* 40 menu_screen.c:644 (arg: app.paths.cheats) */ {"Put .cht files in %s, or one next to the game.",
        "Put .cht files in Tsugaru\\cheats%.0s, or one next to the game.", "Coloque ficheiros .cht em Tsugaru\\cheats%.0s, ou um junto ao jogo."},
    /* 41 menu_screen.c:647 (R2 = Update cheats, :612-621) */ {"tools/SwanStationPS5_sync.py cheats installs the libretro cheat library.",
        "Press {R2} to download this game's cheats from the libretro database.",
        "Prima {R2} para descarregar as batotas deste jogo da base de dados do libretro."},
    /* 42 stats_screen.c:118 */ {"played in SwanStationPS5", "played in Tsugaru", "jogados no Tsugaru"},
    /* 43 profile_screen.c:161 */ {"Not signed in yet: the points show once SwanStationPS5 is online",
        "Not signed in yet: the points show once Tsugaru is online", "Ainda sem sessão iniciada: os pontos aparecem quando o Tsugaru estiver online"},
    /* 44 profile_screen.c:200 */ {"Unlocks earned in SwanStationPS5 show here.", "Unlocks earned in Tsugaru show here.",
        "As conquistas obtidas no Tsugaru aparecem aqui."},
    /* 45 main.c:650 */ {"SwanStationPS5 was updated to the latest version!", "Tsugaru was updated to the latest version!",
        "O Tsugaru foi atualizado para a versão mais recente!"},
    /* 46 tips.c:54 */ {"%d discs: in the game, SwanStationPS5 menu > Disc.", "%d discs: in the game, game menu > Disc.", "%d discos: no jogo, menu do jogo > Disco."},
    /* 47 tips.c:56 */ {"Has a manual: in the game, SwanStationPS5 menu > Manual.", "Has a manual: in the game, game menu > Manual.",
        "Tem manual: no jogo, menu do jogo > Manual."},
    /* 48 assets/game-tips.txt:14 (tips.c:94) */ {"Up to 4 players in SwanStationPS5 with the multitap (Settings > Controls > Players).",
        "Up to 4 players in Tsugaru with the multitap (Settings > Controls > Players).",
        "Até 4 jogadores no Tsugaru com o multitap (Definições > Controlos > Jogadores)."},
    /* 49 assets/game-tips.txt:15-20 */ {"Made for a light gun, which SwanStationPS5 doesn't support yet.",
        "A light gun game: the right stick aims (Settings > Controls > Light gun).",
        "Jogo de pistola de luz: o analógico direito aponta (Definições > Controlos > Pistola de luz)."},
    /* 50 assets/game-tips.txt:21 */ {"On a real console it builds levels from music CDs; SwanStationPS5 can't load audio CDs, so only its own songs play.",
        "On a real console it builds levels from music CDs; Tsugaru can't load audio CDs, so only its own songs play.",
        "Numa consola real cria níveis a partir de CD de música; o Tsugaru não lê CD de áudio, só as músicas do jogo tocam."},

    /* ---- new strings of the SWANUI_XBOX code in this spec (en NULL: the key is the English text) */
    /* 51 coverflow.c empty shelf (5.1) */ {"Games: on a USB drive (NTFS), in a SwanStation or psx folder, one folder per game.", NULL,
        "Jogos: numa unidade USB (NTFS), numa pasta SwanStation ou psx, uma pasta por jogo."},
    /* 52 */ {"On a PC, give ALL APPLICATION PACKAGES read access to the drive; on the console, set it up as Media.", NULL,
        "Num PC, dê a ALL APPLICATION PACKAGES acesso de leitura à unidade; na consola, configure-a como «Media»."},
    /* 53 */ {"Or copy them with Device Portal into LocalState\\Tsugaru\\games.", NULL,
        "Ou copie-os com o Device Portal para LocalState\\Tsugaru\\games."},
    /* 54 */ {"BIOS: your console's BIOS file (SCPH1001.BIN is recommended) in LocalState\\Tsugaru\\bios.", NULL,
        "BIOS: o ficheiro da BIOS da sua consola (recomenda-se SCPH1001.BIN) em LocalState\\Tsugaru\\bios."},
    /* 55 */ {"Then press {L3} to look for games.", NULL, "Depois prima {L3} para procurar jogos."},
    /* 56 settings_screen.c Xbox PAD table (4.6) */ {"LS (left stick)", NULL, "LS (analógico esquerdo)"},
    /* 57 */ {"RS (right stick)", NULL, "RS (analógico direito)"},
    /* 58 Xbox ABOUT table (6.2) */ {"This build", NULL, "Esta versão"},
    /* 59 */ {"Private Xbox port", NULL, "Port privado para Xbox"},
    /* 60 */ {"Credits", NULL, "Créditos"},
    /* 61 */ {"Began as", NULL, "Começou como"},
    /* 62 */ {"PSXS5 by SynoPiia", NULL, "PSXS5, de SynoPiia"},
    /* 63 */ {"Emulation core", NULL, "Núcleo de emulação"},
    /* 64 */ {"Licences", NULL, "Licenças"},
    /* 65 */ {"GPL-3.0+ and each core's own licence", NULL, "GPL-3.0+ e a licença de cada núcleo"},
    /* 66 */ {"Thanks", NULL, "Agradecimentos"},
    /* 67 */ {"Covers, fonts, icons, cheats...", NULL, "Capas, letras, ícones, batotas..."},
    /* 68 */ {"Tsugaru: the SwanStationPS5 interface brought to the Xbox, running on the emulation core named under Credits.", NULL,
        "Tsugaru: a interface do SwanStationPS5 trazida para a Xbox, a correr sobre o núcleo de emulação indicado em Créditos."},
    /* 69 */ {"An unofficial build for personal use on a console in Developer Mode. It is not published and must not be shared: the licences of its parts do not allow them to be distributed together (see Licences).", NULL,
        "Uma versão não oficial para uso pessoal numa consola em Modo de Programador. Não é publicada e não pode ser partilhada: as licenças das suas partes não permitem distribuí-las juntas (ver Licenças)."},
    /* 70 */ {"Never included: copy your own console's BIOS (SCPH1001.BIN is recommended) into Tsugaru\\bios with Device Portal.", NULL,
        "Nunca incluída: copie a BIOS da sua consola (recomenda-se SCPH1001.BIN) para Tsugaru\\bios com o Device Portal."},
    /* 71 */ {"Your own backups: in Tsugaru\\games, or on a USB drive in Tsugaru, Tsugaru\\games or the Games folder on drives.", NULL,
        "As suas cópias: em Tsugaru\\games, ou numa unidade USB em Tsugaru, Tsugaru\\games ou na pasta de jogos nas unidades."},
    /* 72 */ {"The USB drives the app can read, with their free space. A drive must be NTFS, readable by ALL APPLICATION PACKAGES, and set up as Media on the console.", NULL,
        "As unidades USB que a aplicação consegue ler, com o espaço livre. A unidade tem de ser NTFS, legível por ALL APPLICATION PACKAGES e configurada como «Media» na consola."},
    /* 73 */ {"Saves, states, covers and settings live here, in the app's LocalState (Device Portal > File explorer). Uninstalling the app deletes it.", NULL,
        "As gravações, estados, capas e definições ficam aqui, no LocalState da aplicação (Device Portal > File explorer). Desinstalar a aplicação apaga-a."},
    /* 74 */ {"The shelf, menus and tools are SwanStationPS5's, by AngelXex (darkxex) and BlackBearReloaded, GPL-3.0-or-later. This port only changes the buttons, the texts and the parts the Xbox does not have.", NULL,
        "A prateleira, os menus e as ferramentas são do SwanStationPS5, de AngelXex (darkxex) e BlackBearReloaded, GPL-3.0-or-later. Este port só muda os botões, os textos e as partes que a Xbox não tem."},
    /* 75 */ {"SwanStationPS5's interface began as PSXS5's, by SynoPiia (github.com/SynoPiia/PSXS5).", NULL,
        "A interface do SwanStationPS5 começou como a do PSXS5, de SynoPiia (github.com/SynoPiia/PSXS5)."},
    /* 76 */ {"SwanStation frontend, based on DuckStation by Stenzek (Connor McLaughlin) and its contributors (github.com/stenzek/duckstation). This build does not include the DuckStation core.", NULL,
        "Frontend SwanStation, baseado no DuckStation de Stenzek (Connor McLaughlin) e dos seus contribuidores (github.com/stenzek/duckstation). Esta build não inclui o núcleo DuckStation."},
    /* 77 */ {"Interface: GPL-3.0-or-later. Emulation cores: each has its own licence (see the LICENSE files of ares and TOWNSEMU). PlayStation is a trademark of Sony Interactive Entertainment and Xbox of Microsoft; neither is involved.", NULL,
        "Interface: GPL-3.0-or-later. Núcleos de emulação: cada um tem a sua própria licença (veja os arquivos LICENSE do ares e do TOWNSEMU). PlayStation é uma marca da Sony Interactive Entertainment e Xbox da Microsoft; nenhuma está envolvida."},
    /* 78 */ {"RetroAchievements and rcheevos; xlenore/psx-covers and libretro-thumbnails (covers, art); the libretro-database contributors (cheats); sodaraptor's Hypnagogia Boundless Dreams Jewel Case (CC BY 4.0, adapted); Inter, Noto Sans JP and the theme fonts (SIL OFL 1.1); Tabler Icons, stb and Nayuki's QR Code generator. Full list: SwanStationPS5's THIRD_PARTY_NOTICES.md.", NULL,
        "RetroAchievements e rcheevos; xlenore/psx-covers e libretro-thumbnails (capas, arte); os contribuidores da libretro-database (batotas); a caixa Hypnagogia Boundless Dreams Jewel Case de sodaraptor (CC BY 4.0, adaptada); Inter, Noto Sans JP e as letras dos temas (SIL OFL 1.1); Tabler Icons, stb e o QR Code generator de Nayuki. Lista completa: THIRD_PARTY_NOTICES.md do SwanStationPS5."},
    /* 79 Shader labels (4.4) */ {"CRT Hyllian", NULL, NULL},

    /* ---- new toasts and errors of S3 and S4 (S3 Q3), Portuguese added here */
    /* 80 S3 sec. 4 */ {"BIOS not found: copy your console's BIOS file (SCPH1001.BIN is recommended) into the bios folder.", NULL,
        "BIOS não encontrada: copie o ficheiro da BIOS da sua consola (recomenda-se SCPH1001.BIN) para a pasta bios."},
    /* 81 S3 sec. 4 */ {"The previous game is still closing.", NULL, "O jogo anterior ainda está a fechar."},
    /* 82 S3 sec. 8.3 */ {"Rewind is not available while Run-ahead is on", NULL, "Retroceder não está disponível com a Antecipação ligada"},
    /* 83 S4 sec. 6.6 */ {"Not available on Xbox", NULL, "Não disponível na Xbox"},
    /* 84 S4 sec. 3 */ {"RetroAchievements: your sign-in expired, sign in again", NULL, "RetroAchievements: a sua sessão expirou, inicie sessão de novo"},
    /* ---- Tsugaru: the games folder picked with the Xbox folder picker (compat/swanui_xbox_folder.c) */
    /* 85 settings_screen.c */ {"Games folder", NULL, "Pasta de jogos"},
    /* 86 settings_screen.c */ {"Choose the folder with your games with the Xbox file picker. Games can be directly in it or one folder per game.", NULL,
        "Escolha a pasta com os seus jogos no selector de ficheiros da Xbox. Os jogos podem estar directamente nela ou numa pasta por jogo."},
    /* 87 coverflow.c */ {"Choose the folder with your games (USB drive or disk) with the Xbox file picker.", NULL,
        "Escolha a pasta com os seus jogos (pen ou disco USB) no selector de ficheiros da Xbox."},
    /* 88 coverflow.c */ {"Supported: .cue+.bin, .chd, .pbp, .iso, .img, .ecm, .m3u. All in one folder, or one folder per game.", NULL,
        "Suportados: .cue+.bin, .chd, .pbp, .iso, .img, .ecm, .m3u. Todos numa pasta, ou uma pasta por jogo."},
    /* 89 coverflow.c */ {"Current folder: %s", NULL, "Pasta actual: %s"},
    /* 90 coverflow.c */ {"BIOS: included in Tsugaru.", NULL, "BIOS: incluída no Tsugaru."},
    /* 91 coverflow.c */ {"BIOS: put SCPH1001.BIN in LocalState\\Tsugaru\\bios (Device Portal).", NULL,
        "BIOS: coloque SCPH1001.BIN em LocalState\\Tsugaru\\bios (Device Portal)."},
    /* 92 coverflow.c */ {"Press {Cross} to select the games folder", NULL, "Prima {Cross} para escolher a pasta de jogos"},
    /* 93 swanui_xbox_folder.c */ {"Games folder selected: looking for games...", NULL, "Pasta de jogos escolhida: a procurar jogos..."},
    /* 94 swanui_xbox_folder.c */ {"No folder selected", NULL, "Nenhuma pasta escolhida"},
};

#if defined(TSUGARU_UWP)
/* TSUGARU_UWP (FM Towns): texts that say PS1, BIOS, DuckStation or PlayStation in the table above or in the REF literals,
 * rewritten for the FM Towns. Consulted before XBOX_TEXTS, so an entry here wins. Same keys, byte for byte. */
static const XboxText TOWNS_TEXTS[] = {
    {"{Touchpad}: tap = {Select}, hold = menu", "{Touchpad}: tap = Select, hold = shortcuts",
     "{Touchpad}: toque = Select, manter = atalhos"},
    {"Games that support the PlayStation Mouse (Discworld, Lemmings...): slide a finger on the {Touchpad} to move the pointer; {R2} or {Cross} clicks, {L2} or {Circle} right-clicks.",
     "Games that support the mouse (many FM Towns games do): the right stick moves the pointer; {R2} or {Cross} clicks, {L2} or {Circle} right-clicks.",
     "Jogos compatíveis com o rato (muitos jogos do FM Towns): o analógico direito move o ponteiro; {R2} ou {Cross} clica, {L2} ou {Circle} faz clique direito."},
    {"BIOS", "ROM files", "Ficheiros ROM"},
    {"Never included: copy your own console's BIOS (SCPH1001.BIN is recommended) into Tsugaru\\bios with Device Portal.",
     "Built in: the FM Towns system ROMs ship with Tsugaru. Your own set (FMT_SYS.ROM, FMT_DOS.ROM, FMT_FNT.ROM, FMT_DIC.ROM) in LocalState\\Tsugaru\\bios, or in a folder named rom inside the games folder, is used instead when present.",
     "Incluídos: as ROMs de sistema do FM Towns vêm com o Tsugaru. O seu próprio conjunto (FMT_SYS.ROM, FMT_DOS.ROM, FMT_FNT.ROM, FMT_DIC.ROM) em LocalState\\Tsugaru\\bios, ou numa pasta chamada rom dentro da pasta de jogos, é usado em vez delas quando existe."},
    {"BIOS not found: copy your console's BIOS file (SCPH1001.BIN is recommended) into the bios folder.",
     "ROM files not found: the built-in set is missing from the package and none was found in LocalState\\Tsugaru\\bios.",
     "Ficheiros ROM não encontrados: o conjunto incluído falta no pacote e não há nenhum em LocalState\\Tsugaru\\bios."},
    {"Supported: .cue+.bin, .chd, .pbp, .iso, .img, .ecm, .m3u. All in one folder, or one folder per game.",
     "Supported: CD images (.cue+.bin, .iso, .mds+.mdf) and floppy images (.d77, .d88, .img). All in one folder, or one folder per game.",
     "Suportados: imagens de CD (.cue+.bin, .iso, .mds+.mdf) e de disquete (.d77, .d88, .img). Todos numa pasta, ou uma pasta por jogo."},
    {"BIOS: included in Tsugaru.", "ROM: the FM Towns system ROMs are built in.", "ROM: as ROMs de sistema do FM Towns estão incluídas."},
    {"BIOS: put SCPH1001.BIN in LocalState\\Tsugaru\\bios (Device Portal).",
     "ROM: none found. Put FMT_SYS.ROM, FMT_DOS.ROM, FMT_FNT.ROM and FMT_DIC.ROM in LocalState\\Tsugaru\\bios (Device Portal).",
     "ROM: nenhuma encontrada. Ponha FMT_SYS.ROM, FMT_DOS.ROM, FMT_FNT.ROM e FMT_DIC.ROM em LocalState\\Tsugaru\\bios (Device Portal)."},
    /* ---- TSUGARU_UWP branding: Tsugaru is the product; SwanStationPS5 is credited as the interface's origin */
    {"Tsugaru: the SwanStationPS5 interface brought to the Xbox, running on the emulation core named under Credits.",
     "Tsugaru: the FM TOWNS / Marty emulator by Soji Yamakawa on the Xbox, with the shelf and menus of SwanStationPS5 (by darkxex).",
     "Tsugaru: o emulador do FM TOWNS / Marty de Soji Yamakawa na Xbox, com a prateleira e os menus do SwanStationPS5 (de darkxex)."},
    {"An unofficial build for personal use on a console in Developer Mode. It is not published and must not be shared: the licences of its parts do not allow them to be distributed together (see Licences).",
     "An unofficial build for a console in Developer Mode. Interface GPL-3.0-or-later, emulator BSD-3-Clause: if it is passed on, its source code must go with it (GPL). The system ROMs are built in; your games are your own.",
     "Uma versão não oficial para uma consola em Modo de Programador. Interface GPL-3.0-or-later, emulador BSD-3-Clause: se for passada a alguém, o código-fonte tem de ir com ela (GPL). As ROMs de sistema estão incluídas; os jogos são seus."},
    {"The person behind SwanStationPS5.", "Tsugaru's author. The shelf and menus are SwanStationPS5's, by darkxex.",
     "O autor do Tsugaru. A prateleira e os menus são do SwanStationPS5, de darkxex."},
    {"The shelf, menus and tools are SwanStationPS5's, by AngelXex (darkxex) and BlackBearReloaded, GPL-3.0-or-later. This port only changes the buttons, the texts and the parts the Xbox does not have.",
     "Frontend based on SwanStationPS5, by AngelXex (darkxex) and BlackBearReloaded, GPL-3.0-or-later (github.com/darkxex/SwanStationPS5). This port changes the buttons, the texts, the media (CD-ROM and floppy) and the emulator underneath.",
     "Interface baseada no SwanStationPS5, de AngelXex (darkxex) e BlackBearReloaded, GPL-3.0-or-later (github.com/darkxex/SwanStationPS5). Este port muda os botões, os textos, os suportes (CD-ROM e disquete) e o emulador por baixo."},
    {"RetroAchievements and rcheevos; xlenore/psx-covers and libretro-thumbnails (covers, art); the libretro-database contributors (cheats); sodaraptor's Hypnagogia Boundless Dreams Jewel Case (CC BY 4.0, adapted); Inter, Noto Sans JP and the theme fonts (SIL OFL 1.1); Tabler Icons, stb and Nayuki's QR Code generator. Full list: SwanStationPS5's THIRD_PARTY_NOTICES.md.",
     "Kasanova (UNZ) and the Free FM TOWNS Project (compatible ROMs, ysflight.com); sodaraptor's Hypnagogia Boundless Dreams Jewel Case (CC BY 4.0, adapted); Inter, Noto Sans JP and the theme fonts (SIL OFL 1.1); Tabler Icons, stb and Nayuki's QR Code generator. FM TOWNS is a trademark of Fujitsu.",
     "Kasanova (UNZ) e o Free FM TOWNS Project (ROMs compatíveis, ysflight.com); a caixa Hypnagogia Boundless Dreams Jewel Case de sodaraptor (CC BY 4.0, adaptada); Inter, Noto Sans JP e as letras dos temas (SIL OFL 1.1); Tabler Icons, stb e o QR Code generator de Nayuki. FM TOWNS é uma marca da Fujitsu."},
    {"Covers, fonts, icons, cheats...", "ROMs, fonts, icons, the jewel case...", "ROMs, letras, ícones, a caixa..."},
    {"Games: on a USB drive (NTFS), in a SwanStation or psx folder, one folder per game.",
     "Games: on a USB drive (NTFS), in a Tsugaru or fmtowns folder, one folder per game.",
     "Jogos: numa unidade USB (NTFS), numa pasta Tsugaru ou fmtowns, uma pasta por jogo."},
    {"A folder name to look for games in on drives. It is looked for on /mnt/ext0/, /mnt/ext1/ and /mnt/usb0/ to /mnt/usb7/ at once, as the number changes with the drives plugged in: psx searches /mnt/usb0/psx, /mnt/usb1/psx and so on. Leave it empty to turn it off.",
     "A folder name to look for games in at the root of every USB drive (E:, F:...), besides Tsugaru and Tsugaru\\games: fmtowns searches E:\\fmtowns, F:\\fmtowns and so on. The drive must be NTFS and readable by ALL APPLICATION PACKAGES.",
     "Um nome de pasta onde procurar jogos na raiz de cada unidade USB (E:, F:...), além de Tsugaru e Tsugaru\\games: fmtowns procura em E:\\fmtowns, F:\\fmtowns, etc. A unidade tem de ser NTFS e legível por ALL APPLICATION PACKAGES."},
    /* covers: there is no public FM Towns cover repository, so nothing is downloaded */
    {"Fetches covers for new games over the internet.",
     "Covers come from the LaunchBox Games Database (FM Towns Marty). Your own cover.png, or <title>.png, beside the game or in LocalState\\Tsugaru\\covers wins.",
     "As capas vêm da LaunchBox Games Database (FM Towns Marty). Um cover.png, ou <título>.png, junto ao jogo ou em LocalState\\Tsugaru\\covers tem prioridade."},
    {"No images yet: put .png or .jpg files in /data/SwanStationPS5/covers/",
     "No images yet: put cover.png (or <title>.png) beside the game, or .png/.jpg files in LocalState\\Tsugaru\\covers (Device Portal)",
     "Ainda sem imagens: ponha cover.png (ou <título>.png) junto ao jogo, ou ficheiros .png/.jpg em LocalState\\Tsugaru\\covers (Device Portal)"},
    {"Shows the game's own artwork around a 4:3 picture instead of the border, when there is one (downloaded with the covers).",
     "Shows the game's own artwork around a 4:3 picture instead of the border, when a bezel.png lies beside the game.",
     "Mostra a arte do próprio jogo à volta de uma imagem 4:3 em vez da moldura, quando há um bezel.png junto ao jogo."},
    {"Scan for the GitHub page", "Scan for the interface's GitHub page", "Leia para a página GitHub da interface"},
    {"GPL-3.0+ and each core's own licence", "GPL-3.0+ and BSD-3-Clause", "GPL-3.0+ e BSD-3-Clause"},
    {"SwanStation frontend, based on DuckStation by Stenzek (Connor McLaughlin) and its contributors (github.com/stenzek/duckstation). This build does not include the DuckStation core.",
     "Tsugaru (TOWNSEMU), the FM TOWNS / Marty emulator by Soji Yamakawa (CaptainYS) and its contributors (github.com/captainys/TOWNSEMU), 3-clause BSD licence. Built from source with a UWP adaptation; it runs the games and their sound.",
     "Tsugaru (TOWNSEMU), o emulador do FM TOWNS / Marty de Soji Yamakawa (CaptainYS) e dos seus contribuidores (github.com/captainys/TOWNSEMU), com licença BSD de 3 cláusulas. Compilado a partir do código-fonte com uma adaptação UWP; corre os jogos e o seu som."},
    {"Interface: GPL-3.0-or-later. Emulation cores: each has its own licence (see the LICENSE files of ares and TOWNSEMU). PlayStation is a trademark of Sony Interactive Entertainment and Xbox of Microsoft; neither is involved.",
     "Interface: GPL-3.0-or-later. Emulation core: BSD-3-Clause. FM TOWNS is a trademark of Fujitsu and Xbox of Microsoft; neither is involved. An unofficial build; the games and ROM files are yours.",
     "Interface: GPL-3.0-or-later. Núcleo de emulação: BSD-3-Clause. FM TOWNS é uma marca da Fujitsu e Xbox da Microsoft; nenhuma está envolvida. Uma versão não oficial; os jogos e os ficheiros ROM são seus."},
};
#endif

const char *swanui_xbox_tr(const char *english, int lang)
{
    if (!english || !english[0])
        return NULL;
#if defined(TSUGARU_UWP)
    for (size_t i = 0; i < sizeof(TOWNS_TEXTS) / sizeof(TOWNS_TEXTS[0]); ++i)
    {
        const XboxText *t = &TOWNS_TEXTS[i];
        if (t->key[0] != english[0] || strcmp(t->key, english) != 0)
            continue;
        if (lang == LANG_PT && t->pt)
            return t->pt;
        return t->en ? t->en : t->key;
    }
#endif
    /* a linear walk: about 90 rows, first-character filter; no lazy hash, so worker threads may call it too */
    for (size_t i = 0; i < sizeof(XBOX_TEXTS) / sizeof(XBOX_TEXTS[0]); ++i)
    {
        const XboxText *t = &XBOX_TEXTS[i];
        if (t->key[0] != english[0] || strcmp(t->key, english) != 0)
            continue;
        if (lang == LANG_PT && t->pt)
            return t->pt;
        return t->en ? t->en : t->key;
    }
    return NULL;
}

/* ---- Tsugaru: the product name on screen (TSUGARU_UWP: BRAND_TO is the emulator's name) ----
 * The REF texts name SwanStationPS5 in many places (toasts, help, the About card). Rather than a hunk per text,
 * ui/text.c passes every string it measures or draws through here. Results are cached by content, since the
 * strings come from literals and from stack buffers alike; the table never evicts (a pointer handed out stays
 * valid), and when it is full the original text is returned. A spin lock makes it safe for worker threads. */
#include <windows.h>
#include <stdbool.h>
#include <stdlib.h>

static const char *const BRAND_FROM[] = {"SwanStationPS5", "SwanStation", "PSXS5"};
static const char BRAND_TO[] = "Tsugaru"; /* TSUGARU_UWP: the emulator's name */

typedef struct
{
    char *from, *to;
} BrandEntry;

static BrandEntry s_brand[192];
static int s_brand_count;
static volatile LONG s_brand_lock;

static bool brand_needed(const char *s)
{
    if (strstr(s, "darkxex") || strstr(s, "SynoPiia") || strstr(s, "THIRD_PARTY"))
        return false; /* TSUGARU_UWP: the credits name the original frontend as it is */
    return strstr(s, "SwanStation") != NULL || strstr(s, "PSXS5") != NULL;
}

/* s with every BRAND_FROM replaced by BRAND_TO; malloc'd, or NULL */
static char *brand_rewrite(const char *s)
{
    size_t cap = strlen(s) * 2 + 64;
    char *out = (char *)malloc(cap);
    if (!out)
        return NULL;
    size_t n = 0;
    while (*s)
    {
        size_t skip = 0;
        for (size_t i = 0; i < sizeof(BRAND_FROM) / sizeof(BRAND_FROM[0]) && !skip; ++i)
        {
            const size_t len = strlen(BRAND_FROM[i]);
            if (strncmp(s, BRAND_FROM[i], len) == 0)
                skip = len;
        }
        const char *piece = skip ? BRAND_TO : s;
        const size_t piece_len = skip ? sizeof(BRAND_TO) - 1 : 1;
        if (n + piece_len + 1 > cap)
        {
            cap = cap * 2 + piece_len;
            char *grown = (char *)realloc(out, cap);
            if (!grown)
            {
                free(out);
                return NULL;
            }
            out = grown;
        }
        memcpy(out + n, piece, piece_len);
        n += piece_len;
        s += skip ? skip : 1;
    }
    out[n] = '\0';
    return out;
}

const char *swanui_xbox_brand(const char *s)
{
    if (!s || !brand_needed(s))
        return s;
    while (InterlockedCompareExchange(&s_brand_lock, 1, 0) != 0)
        ;
    const char *result = s;
    int i = 0;
    for (; i < s_brand_count; ++i)
        if (strcmp(s_brand[i].from, s) == 0)
            break;
    if (i < s_brand_count)
        result = s_brand[i].to;
    else if (s_brand_count < (int)(sizeof(s_brand) / sizeof(s_brand[0])))
    {
        char *from = _strdup(s), *to = brand_rewrite(s);
        if (from && to)
        {
            s_brand[s_brand_count].from = from;
            s_brand[s_brand_count].to = to;
            ++s_brand_count;
            result = to;
        }
        else
        {
            free(from);
            free(to);
        }
    }
    InterlockedExchange(&s_brand_lock, 0);
    return result;
}
