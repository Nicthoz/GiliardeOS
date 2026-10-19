# GiliardeOS v1.0 - mini SO 8-bit para Arduino (ATmega328)

## Como usar
1. Abra a pasta `GiliardeOS` no Arduino IDE (abra o arquivo `GiliardeOS.ino`).
2. Placa: Arduino Uno ou Nano (ATmega328P). Nenhuma biblioteca extra e necessaria
   (usa so `Wire` e `EEPROM`, que ja vem com a IDE).
3. Monte a ligacao abaixo e clique em Carregar.

## Ligacao
| Peca            | Pino do Arduino                    |
|-----------------|------------------------------------|
| OLED VCC / GND  | 5V / GND                           |
| OLED SDA / SCL  | A4 / A5                            |
| Botao CIMA      | D2  (outro lado do botao no GND)   |
| Botao BAIXO     | D3                                 |
| Botao ESQUERDA  | D4                                 |
| Botao DIREITA   | D5                                 |
| Botao A         | D6                                 |
| Botao B         | D7                                 |
| Buzzer (+)      | D8  (outro lado no GND)            |

Os botoes usam o pull-up interno: nao precisa de resistor.

## Controles
- Menu: ESQ/DIR escolhem, **A** abre.
- Em qualquer app: segure **A + B** por 0,6 s para voltar ao menu.
- **Tetris**: ESQ/DIR move, CIMA ou A gira, BAIXO desce, B queda rapida.
- **Pacman**: setas mudam a direcao (a curva fica na fila).
- **Doom**: CIMA/BAIXO anda, ESQ/DIR gira, B+ESQ/DIR anda de lado, A atira.
- **System**: CIMA liga/desliga o som, segure A por 2 s para zerar os recordes.

## Problemas comuns
- Tela apagada: troque `OLED_ADDR` para `0x3D` em `gos_config.h`.
- Imagem deslocada 2 pixels / lixo nas bordas: o modulo e SH1106 em vez de SSD1306.
- Sem som: use buzzer passivo (ou ativo) entre D8 e GND.

## Simulador no PC (sem Arduino)
`python gos_sim.py` abre uma janela que imita o OLED + um terminal de comandos (`help`).
Setas = direcional, Z = A, X = B, Esc = sair do app. `python gos_sim.py --test` faz um teste automatico.
(O arquivo gos_sim.py fica dentro da pasta, mas o Arduino IDE ignora arquivos .py.)
