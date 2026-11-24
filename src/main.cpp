#include <Arduino.h>

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include <qrcode.h>

#define PIN_BOTAO_POSITIVO 14
#define PIN_BOTAO_NEUTRO 26
#define PIN_BOTAO_NEGATIVO 27
#define PIN_CLK 32
#define PIN_DT 33

#define TFT_CS 5
#define TFT_DC 2
#define TFT_RST 4

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

volatile int avaPositiva = 0;
volatile int avaNeutra = 0;
volatile int avaNegativa = 0;

volatile unsigned long ultimoClique = 0;

volatile int avaliacoesPendentes = 0;
volatile int passosEncoder = 0;
volatile unsigned long ultimoTempoCredito = 0;

volatile bool tentouSemCredito = false;

unsigned long tempoAviso = 0;
bool avisoAtivo = false;

const char* URL_AVALIACAO = "https://ru.ufrn.br/voto";


// Funções qrcode

void desenhaQrCode(const char* texto){
  //gera a estrutura do qrCode e reserva espaço na RAM para a versão 2 -qrcode_getBufferSize(2)-
  QRCode qrcode;
  uint8_t qrCodeData[qrcode_getBufferSize(2)];

  qrcode_initText(&qrcode, qrCodeData, 2, ECC_LOW, texto); // gera a matriz binária a partir do texto recebido

  //desenha o fundo branco do qrCode centralizando na janela do meio do display
  int escala = 7; // cada modulo do qrCode tem 7x7 pixels 
  int margem = 4;
  int tamanhoTotal = (qrcode.size * escala) + (margem*2);
  int fundoX = (240 - tamanhoTotal) / 2;
  int fundoY = 82;

  tft.fillRect(0, 78, 240, 190, ILI9341_BLACK);
  tft.fillRect(fundoX, fundoY, tamanhoTotal, tamanhoTotal, ILI9341_WHITE);

  // percorre linha e coluna da matriz e desenha seus valores no display 
  for(uint8_t y = 0; y < qrcode.size; y++){
    for (uint8_t x = 0; x < qrcode.size; x++){
      if(qrcode_getModule(&qrcode, x, y)){ // se o modulo da posição (x,y) da matriz for 1, desenha um quadrado preto no display
        tft.fillRect(
          fundoX + margem + (x * escala),
          fundoY + margem + (y * escala),
          escala, escala, ILI9341_BLACK
        );
      }
    }
  }
}


// funções de interação com botões

void IRAM_ATTR precionarBotao(void* arg){ // IRAM_ATTR diz ao compilador para guardar a função na memória RAM ao invés da flash
  volatile int* voto = (volatile int*) arg; //volatile: força o compilador a ler e gravar direto na memoria RAM evitanto a cópia de variáveis para a memoria interna
  unsigned long tempoAtual = millis(); // unsigned long: aceita somente números positivos(estou registrando tempo). Como os valores negativos de double não são considerados eu consigo mais memória para dobrar sua capacidade de armazenamento de positivos
  if(tempoAtual - ultimoClique > 500){
    if(avaliacoesPendentes > 0){
      (*voto)++;
      avaliacoesPendentes--;
      ultimoTempoCredito = millis();
    } else{
      tentouSemCredito = true;
    }
    ultimoClique = tempoAtual;
  }
}

// função catraca 

void IRAM_ATTR aoGirarEncoder(){
  if(digitalRead(PIN_DT) == HIGH) passosEncoder++;

  if(passosEncoder >= 7){
    avaliacoesPendentes++;
    passosEncoder = 0;
    ultimoTempoCredito = millis();
  }
}

// Funções do display

void desenharCabecalho() {
  // Retângulo azul no topo: x=0, y=0, largura=240, altura=50
  tft.fillRect(0, 0, 240, 50, ILI9341_NAVY);
  
  tft.setCursor(12, 6);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN);
  tft.println("TURNO: ALMOCO");

  tft.setCursor(12, 28);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.println("Frango Grelhado");
}

void atualizarCreditos() {
  tft.fillRect(0, 54, 240, 22, ILI9341_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_YELLOW);

  tft.setCursor(30, 58);
  tft.print("Ava_pendente: ");
  tft.print(avaliacoesPendentes);
}

void mostrarConfirmacao() {
  tft.fillRect(0, 272, 240, 48, ILI9341_DARKGREEN);
  tft.drawRect(0, 272, 240, 48, ILI9341_GREEN);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);

  tft.setCursor(66, 278);
  tft.print("Avaliacao");

  tft.setCursor(54, 298);
  tft.print("registrada!");

  avisoAtivo = true;
  tempoAviso = millis();
}

void mostraAviso() {
  tft.fillRect(0, 272, 240, 48, ILI9341_ORANGE);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_BLACK);

  tft.setCursor(54, 288);
  tft.print("Ja avaliou!");

  avisoAtivo = true;
  tempoAviso = millis();
}

void limparAviso() {
  tft.fillRect(0, 272, 240, 48, ILI9341_BLACK);
  avisoAtivo = false;
}

void setup() {
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);
  desenharCabecalho();
  atualizarCreditos();

  desenhaQrCode(URL_AVALIACAO);

  Serial.begin(115200); // Inicia a comunicação serial com o computador na velocidade de 115.200 bits por segundo
  pinMode(PIN_BOTAO_POSITIVO, INPUT_PULLUP); // INPUT_PULLUP garante que o botão não "clique" sozinho com interferências eletromagnéticas, considerando somente HIGH ou LOW
  pinMode(PIN_BOTAO_NEUTRO, INPUT_PULLUP);
  pinMode(PIN_BOTAO_NEGATIVO, INPUT_PULLUP);

  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);

  // attachInterruptArg: pausa o código principal e executa uma função de imediato quando ocorre uma mudança de estado no pino
  attachInterruptArg(digitalPinToInterrupt(PIN_BOTAO_POSITIVO), precionarBotao, (void*)&avaPositiva, FALLING); // FALLING dispara o evento assim que o botão é preciosado. HIGH -> LOW
  attachInterruptArg(digitalPinToInterrupt(PIN_BOTAO_NEUTRO), precionarBotao, (void*)&avaNeutra, FALLING);
  attachInterruptArg(digitalPinToInterrupt(PIN_BOTAO_NEGATIVO), precionarBotao, (void*)&avaNegativa, FALLING);

  attachInterrupt(digitalPinToInterrupt(PIN_CLK), aoGirarEncoder, FALLING);
}

void loop() {
  static int numAvaliacoes_antigo = -1; 
  static int dadosCatraca_antigo  = -1; // teste

  int numAvaliacoes = (avaPositiva + avaNeutra + avaNegativa);
  int dadosCatraca = avaliacoesPendentes; // teste (passosEncoder + avaliacoesPendentes)

  if(avaliacoesPendentes > 0 && millis() - ultimoTempoCredito >= 20000){
    avaliacoesPendentes = 0; 
  }

  if(numAvaliacoes != numAvaliacoes_antigo || dadosCatraca_antigo != dadosCatraca){

    if(numAvaliacoes > numAvaliacoes_antigo && numAvaliacoes_antigo != -1){
      mostrarConfirmacao();
    }

    numAvaliacoes_antigo = numAvaliacoes;
    dadosCatraca_antigo = dadosCatraca;

    atualizarCreditos();

    Serial.print("===================================== \r\n");
    Serial.printf("Créditos: %d \r\n", avaliacoesPendentes);
    Serial.printf("Passos Encoder: %d \r\n", passosEncoder);
    Serial.printf("\nAvaliações positivas: %d \r\n", avaPositiva);
    Serial.printf("Avaliações neutras: %d \r\n", avaNeutra);
    Serial.printf("Avaliações negativas %d \r\n", avaNegativa);
    Serial.print("===================================== \r\n\n");
  }

  if(tentouSemCredito){
    tentouSemCredito = false;
    mostraAviso();
  }

  if (avisoAtivo && (millis() - tempoAviso >= 2000)) {
    limparAviso();
  }

  delay(10);
}