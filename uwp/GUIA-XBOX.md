# Tsugaru (FM Towns / Marty) no Xbox - guia do pacote

Port UWP do emulador **Tsugaru** (TOWNSEMU, BSD-3, de Soji Yamakawa) com a interface do SwanStationPS5 (GPL-3)
rebatizada como Tsugaru, tema "Marty" e o mascote como ícone. Pacote de uso **pessoal**: as ROMs do sistema vão
dentro dele, portanto nunca o distribua.

## Arquivos

- `E:\claude-work\townsuwp\out\Tsugaru-FMTowns-Xbox-x64-0.1.0.0.msix` (assinado com `CN=irixaligned`)
- `Tsugaru-Dev.cer` (certificado público) e `Dependencies\Microsoft.VCLibs.x64.14.00.appx`

## Instalar no Xbox (modo desenvolvedor)

1. Device Portal > **Add** > envie o `.msix`; se pedir dependências, envie antes o VCLibs.
2. Em **View details** do app, tipo **Game**, e reinicie o app.

## ROMs e jogos

- As ROMs (`FMT_SYS/DOS/FNT/DIC/F20.ROM`) estão **embutidas** no pacote. Para usar outro conjunto, crie
  `LocalState\Tsugaru\bios\` com os arquivos (ou uma pasta `rom` dentro da pasta de jogos); esses têm prioridade.
- Jogos: imagens de CD (`.cue/.bin`, `.iso`, `.mds/.mdf`) e disquetes (`.d77`, `.d88`, `.img`), numa pasta do HD
  interno ou USB. Na prateleira vazia, **A** abre o seletor de pasta do Xbox.
- Jogos com vários discos: a biblioteca cria um `.m3u` com os "(Disc n)"; a troca de disco é pelo menu do jogo.
- Um CD pode ter um disquete ao lado (nome começando igual): ele entra no drive A automaticamente.
- Capas, telas de título e snaps: baixados da LaunchBox Games Database (plataforma "Fujitsu FM Towns Marty",
  604 capas), pelo nome do jogo. Um `cover.png` (ou `<título>.png`) ao lado do jogo ou em `LocalState\Tsugaru\covers`
  tem prioridade. Desligue em Configurações > "Baixar capas", ou crie `LocalState\disable-net.txt`.

## Controles

| Xbox | FM Towns |
|---|---|
| A / X | botão A |
| B / Y | botão B |
| Menu | RUN |
| View | SELECT |
| D-pad / analógico esquerdo | direcional |
| L3 + R3 (segurar) | menu do jogo (pausa, estados, trocar disco, sair) |

Dois controles = as duas portas. Teclado e mouse USB/Bluetooth vão direto para o FM Towns durante o jogo.

## O que funciona (testado no PC, Windows 10, com o Last Armageddon CD Special)

- Boot do CD real com as ROMs embutidas, 60 fps, menu do título.
- Estados em memória (rewind e retomada rápida ao sair) e estados em arquivo: autoteste `serialize ok, unserialize ok,
  file save ok, file load ok`; o `.resume` com miniatura foi gravado sozinho.
- Caminhos com acentos (UTF-8) na leitura e escrita; no Xbox, pastas escolhidas pelo seletor passam pelo broker.
- Configurações de imagem: brilho, cor (natural/vívido/suave/quente/frio/P&B), nitidez, shaders (CRT, LCD, NTSC),
  FSR, prescale (Scale2x / xBR).
- Tema padrão "Marty" (fundo claro, azul do logo); nome "Tsugaru" em todas as telas.

Não testado: Xbox real, som de jogo, controle físico, teclado/mouse durante o jogo.

## Diagnóstico

Arquivos em `LocalState` (Device Portal > File explorer):

- `startup-trace.txt`: marcos da inicialização e exceções. `tsugaru-uwp.log`: log da interface e do emulador.
- `launch.txt` com o caminho de um jogo: o app abre esse jogo direto ao iniciar (apague depois).
- `selftest-state.txt` (vazio): com um jogo rodando há ~10 s, o app testa estados em memória e em arquivo e grava
  uma linha `selftest:` no log.

## Correções desta versão

- Botão **B** fechava o app: no Xbox ele dispara o "voltar" do sistema e, sem tratamento, o shell fecha o app como
  numa página web. Agora o pedido é consumido e as teclas de gamepad/navegação são marcadas como tratadas.
- "Reconhecer todas as ROMs de uma vez": as ROMs do sistema estão embutidas (nada a enviar pelo Device Portal, que só
  aceita um arquivo por vez); e uma pasta de jogo aceita até 256 arquivos (CDs com 60+ faixas).

## Limitações restantes

- MIDI e porta serial por rede ficam desligados no UWP.
- O QR da tela Sobre ainda aponta para a página do SwanStationPS5 (sem gerador de QR no build).
