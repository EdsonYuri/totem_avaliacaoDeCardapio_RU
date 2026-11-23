#include <Arduino.h>

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

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
  // Retângulo azul no topo: x=0, y=0, largura=240, altura=75
  tft.fillRect(0, 0, 240, 75, ILI9341_NAVY);
  
  tft.setCursor(12, 14);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN);
  tft.println("TURNO: ALMOCO");

  tft.setCursor(12, 42);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.println("Frango Grelhado");
}

void atualizarCreditos() {
  tft.fillRect(0, 82, 240, 50, ILI9341_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_YELLOW);


  tft.setCursor(60, 88);
  tft.print("Avaliacoes");

  tft.setCursor(48, 110);
  tft.print("Pendentes: ");
  tft.print(avaliacoesPendentes);
}

void mostrarConfirmacao() {
  tft.fillRect(10, 240, 220, 70, ILI9341_DARKGREEN);
  tft.drawRect(10, 240, 220, 70, ILI9341_GREEN);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);

  tft.setCursor(66, 255);
  tft.print("Avaliacao");

  tft.setCursor(54, 280);
  tft.print("registrada!");

  avisoAtivo = true;
  tempoAviso = millis();
}

void mostraAviso() {
  tft.fillRect(10, 240, 220, 70, ILI9341_ORANGE);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_BLACK);

  tft.setCursor(30, 267);
  tft.print("Ja avaliou!");

  avisoAtivo = true;
  tempoAviso = millis();
}

void limparAviso() {
  tft.fillRect(10, 240, 220, 70, ILI9341_BLACK);
  avisoAtivo = false;
}

void setup() {
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);
  desenharCabecalho();
  atualizarCreditos();

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