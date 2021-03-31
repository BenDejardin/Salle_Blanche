//****************************************************************
//*                                                              *
//*                        Salle Blanche                         *
//*                                                              *
//****************************************************************

// -------------------------------------
// DESCRIPTION MATERIELLE & LOGICIELLE :
// -------------------------------------

// Le système mesure la pression au moyen d'un module BMP280.
// La ventilation d'extraction est piloté proportionnellement
// à la différence de pression.


// ----------------------------------------------------------------
// INCLUDES :
// ----------------------------------------------------------------

// Librairies du capteur BMP280 :
#include "Barometer.h"
#include <Wire.h>

// Librairie du LCD :
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// Import pour gestion du module RTC :
#include "RTClib.h"



// ----------------------------------------------------------------
// VARIABLES GLOBALES & STRUCTURES DE DONNEES
// ----------------------------------------------------------------

// CONSTANTES - Affectation des E/S :
// ----------------------------------
const byte PIN_SENS_1 = 4;  // Pin sens du B.R. para 1.
const byte PIN_INC_1 = 2; // Pin impulsion du B.R. para 1.
const byte PIN_SENS_2 = 5;  // Pin sens du B.R. para 2.
const byte PIN_INC_2 = 3; // Pin impulsion du B.R. para 2.
const byte PIN_SELECT = 6;  // Pin du B.P. sélection/réglage
const byte PIN_AM = 7;    // Pin du B.P. Auto/Manu
const byte PIN_PWM = 9;   // Pin PWM de la carte driver du moteur de ventilateur.


// CONSTANTES - Paramètres du programmes :
// ---------------------------------------
unsigned long DT_MAJ_LCD = 250;   // Période de rafraîchissement de l'écran LCD.
unsigned long DT_MAJ_BP = 300;    // Période d'acquisition des boutons poussoirs.



// Instances (déclarations) des objets le librairies :
// --------------------------------------------------
LiquidCrystal_I2C lcd(0x27,16,2); // Choix de l'adressse I2C 0x27 et configuration des 2 lignes sur 16 caractères.
Barometer myBarometer;
RTC_DS1307 RTC;           //Classe RTC_DS1307


// Variables globles :
// -------------------
volatile int para01 = 20;   // Variable associée au bouton de paramétrage 01.
volatile int para02 = 1050;   // Variable associée au bouton de paramétrage 02.

byte tempManu = para01;     // Consigne de température en mode manuel.
byte tempConf = 22;       // Consigne de température en mode auto/confort.
byte tempEco = 18;        // Consigne de température en mode auto/économique.
float deltaTemp = 0;      // Ecart sur la température.
  
word presManu = para02;     // Consigne de pression en mode manuel.
word presConf = 1080;     // Consigne de pression en mode auto/confort.
word presEco = 1040;      // Consigne de pression en mode auto/économique.
int deltaPresIdeale = 0;        // Ecart sur la pression idéale.
int deltaPresMesurer = 0;       // Ecart sur la pression mesurer.
  
byte numMode = 1;       // Numéro du mode sélectionné.
byte consgineTemp;        // Consigne de température appliquée selon le mode (tempManu ou tempConf ou tempEco)
word consignePres;              // Consigne de pression appliquée selon le mode (presManu ou presConf ou presEco)
boolean modeConfort = true;   // Flag de bascule mode confort/économique.
boolean modeAuto = true;    // Flag de bascule du mode automatique/manuel.
  
unsigned long derDateLCD = 0; // Date du dernier traitement de l'écran LCD.
unsigned long derDateBP = 0;  // Date de la dernière acquisition des BP.
unsigned long derDateCom = 0; // Date de la dernière communication de commande.
 
byte hConf = 8;         // Heure de bascule en mode confort.
byte mnConf = 0;        // Minute de l'heure de bascule en mode confort.
byte hEco = 19;         // Heure de bascule en mode éco.
byte mnEco = 0;         // Minute de l'heure de bascule en mode éco.


// Variables de mesure :
byte h = 0;           // Valeur des heures de l'heure courante de l'horloge RTC.
byte mn = 0;          // Valeur des minutes de l'heure courante de l'horloge RTC.
float temp = 0;         // Température mesurée.
word pres = 0;          // Pression mesurée.


// Etat des boutons poussoirs :
boolean etatBP_Select = false;
boolean etatBP_AM = false;


//________________________________________________________________
//
// SECTION PROGRAMME PRINCIPAL 
//________________________________________________________________

// ---------------------------------------------------------------
// SETUP()
// ---------------------------------------------------------------

void setup(){

  // Configuration des E/S : 
  // ----------------------- 
  pinMode(PIN_SENS_1, INPUT);
  pinMode(PIN_INC_1, INPUT);
  pinMode(PIN_SENS_2, INPUT);
  pinMode(PIN_INC_2, INPUT); 
  pinMode(PIN_SELECT, INPUT);
  pinMode(PIN_AM, INPUT);
  pinMode(PIN_PWM, OUTPUT);

  // Activation des interruptions :
  attachInterrupt(digitalPinToInterrupt(PIN_INC_1), updatePara_1, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_INC_2), updatePara_2, RISING);


  // Initialisations :
  //------------------
  Serial.begin(9600);
  myBarometer.init();
  RTC.begin(); // Démarrage de la librairie RTClib.h  
  lcd.init();  
  lcd.backlight();
  
  // Message de démarrage sur LCD :
  // ------------------------------
  lcd.setCursor(6,0);
  lcd.print("Salle");
  lcd.setCursor(5, 1);
  lcd.print("blanche");
  delay(500);


  // Mise à l'heure et date du module RTC :
  // --------------------------------------
  RTC.adjust(DateTime(__DATE__, __TIME__)); // Met à l'heure et date à laquelle le sketch est compilé.
  //RTC.adjust(DateTime("Dec  5 2012","12:00:00")); 
  //RTC.adjust(DateTime(2017, 5, 3, 15, 25, 0)); // AAAA, MM, JJ, HH, MM, SS
  
} //Fin setup()



// ---------------------------------------------------------------
// LOOP()
// ---------------------------------------------------------------

void loop(){

  // -------------------------------------
  // Extraction de la date et de l'heure :
  // -------------------------------------
  h = RTC.now().hour();
  mn = RTC.now().minute();

  // --------------------------------------------------
  // Acquisition de la températeure et de la pression :
  // --------------------------------------------------
  temp = myBarometer.bmp085GetTemperature( myBarometer.bmp085ReadUT() );
  pres = myBarometer.bmp085GetPressure( myBarometer.bmp085ReadUP() )/100;


  // -------------------------------
  // Gestion des boutons poussoirs :
  // -------------------------------    
   if( (millis() - derDateBP) >= DT_MAJ_BP ){
      gestionBP();
      derDateBP = millis(); // MAJ de la date de traitement.  
      } // Fin if()
  

  // ----------------------------
  // Gestion de l'affichage LCD :
  // ----------------------------  
   if( (millis() - derDateLCD) >= DT_MAJ_LCD ){
      affichageLCD();
    Serial.println("Para 01 : " + String(para01));
    Serial.println("Para 02 : " + String(para02));
      Serial.println("Num. mode : " + String(numMode));
    Serial.println("Temperature BMP : " + String(temp));
      Serial.println("Pression BMP : " + String(pres));
      derDateLCD = millis(); // MAJ de la date de traitement. 
    } // Fin if()


  // --------------------------------
  // Gestion du moteur d'extraction :
  // --------------------------------
  
   ventilateur();
  
  // Cas mode manuel (numMode == 1):
  // -------------------------------
    // Récupération de la consigne selon le mode sélectionné dans l'IHM :

    // Régulation de la pression sur la consigne :



  // Cas automatique créneau confort (numMode != 1 && modeConfort):
  // --------------------------------------------------------------
   // Récupération de la consigne selon le mode sélectionné dans l'IHM :

   // Régulation de la pression sur la consigne :



  // Cas automatique créneau confort (numMode != 1 && !modeConfort):
  // ---------------------------------------------------------------
   // Récupération de la consigne selon le mode sélectionné dans l'IHM :

   // Régulation de la pression sur la consigne :

  


} //Fin loop()


//________________________________________________________________
//
// FONCTIONS DU PROGRAMME
//________________________________________________________________

// ---------------------------------------------------------------
// FONCTIONS D'INTERRUPTION
// ---------------------------------------------------------------

void updatePara_1(){
  // Fonction exéctuée à chaque impulsion sur le bouton rotatif 01.
  // La variable  

  // Routine de la fonction :
  // ------------------------    
  if( digitalRead(PIN_SENS_1) ) para01++;
  else para01--;
 
  } // Fin de updatePara_1()


void updatePara_2(){
  // Fonction exéctuée à chaque impulsion sur le bouton rotatif 02.
  // La variable  

  // Routine de la fonction :
  // ------------------------    
  if( digitalRead(PIN_SENS_2) ) para02++;
  else para02--;
 
  } // Fin de updatePara_2()



// ---------------------------------------------------------------
// gestionBP
// ---------------------------------------------------------------
    
void gestionBP(){
  // Fonction qui assurent l'acquisition et les actions associées aux
  // boutons poussoirs de l'I.H.M.
    
 
   // Acquisition des boutons poussoirs :
   // -----------------------------------
   etatBP_Select = digitalRead(PIN_SELECT); 
   etatBP_AM = digitalRead(PIN_AM);
      
  

   // Bascule du numéro du mode & et association des variables à para1 et para2 :
   // ---------------------------------------------------------------------------
   
// Retour au mode manuel :
  if(etatBP_AM){
      numMode = 1;
      }

   // Bascule des modes :
   if(etatBP_Select){
    numMode = numMode + 1;
    if(numMode == 8) numMode = 2;
    
    // Association des paramètres para1 et para 2 aux variables des modes :
        if(numMode == 1){
      para01 = tempManu;
      para02 = presManu;
      }

        if(numMode == 3){
      para01 = hConf;
      para02 = mnConf;
      }

        if(numMode == 4){
      para01 = tempConf;
      para02 = presConf;
      }

        if(numMode == 5){
      para01 = hEco;
      para02 = mnEco;
      }

        if(numMode == 6){
      para01 = tempEco;
      para02 = presEco;
      }

        if(numMode == 7){
      para01 = h;
      para02 = mn;
      }
    }

   // On borne les valeurs des modes selon leur nature (heure, minute, pression ou température):
   if(numMode == 1 || numMode == 4 || numMode == 6){
    if(para01 > 35) para01 = 35;
    if(para01 < 5) para01 = 5;
    if(para02 > 1300) para02 = 1300;
    if(para02 < 900) para02 = 900;
    }

   if(numMode == 3 || numMode == 5 || numMode == 7){
    if(para01 > 23) para01 = 0;
    if(para01 < 0) para01 = 23;
    if(para02 > 59) para02 = 0;
    if(para02 < 0) para02 = 59;
    }

   // Mise à jour des valeurs à partir des valeurs avec les boutons rotatifs selon le mode :
   if( numMode == 1 ){
    tempManu = para01;
    presManu = para02;
        }
    
   if( numMode == 3 ){
    hConf = para01;
    mnConf = para02;
      }
   
   if( numMode == 4 ){
       tempConf = para01;
       presConf = para02;  
       }

   if( numMode == 5 ){
      hEco = para01;
      mnEco = para02;
      }
   
   if( numMode == 6 ){
      tempEco = para01;
      presEco = para02;      
      }

   if( numMode == 7 ){
   h = para01;
   mn = para02;
   RTC.adjust(DateTime(RTC.now().year(), RTC.now().month(), RTC.now().day(), h, mn, 0)); // AAAA, MM, JJ, HH, MM, SS
   }   


} // Fin de la fonction.



// ---------------------------------------------------------------
// affichageLCD
// ---------------------------------------------------------------

void affichageLCD(){
   // Fonction qui gère l'affichage de l'écran LCD.  
   
   // Variables locales :
   // -------------------
   String heureConf = formatHeure(hConf, mnConf);
   String heureEco = formatHeure(hEco, mnEco);
   String heure = formatHeure(h, mn);
  
   // Routine de la fonction :
   // ------------------------    
   lcd.clear();
   lcd.home();
      
   switch(numMode){
   
   // Ecran Manu :
   case 1:
      lcd.print("Temp");
      lcd.setCursor(5, 0);
      lcd.print(heure);
      lcd.setCursor(12, 0);
      lcd.print("Pres");
      lcd.setCursor(0, 1);
      lcd.print(tempManu);
      lcd.setCursor(3, 1);
      lcd.print("C<-manu->");
      lcd.setCursor(12, 1);
      lcd.print(presManu);
      break;
     

   // AFFICHAGE DE L'ETAT DU MODE AUTOMATIQUE : 
   // Affichage état de la période et valeurs courantes de température et pression
   case 2:
   lcd.print("Temp");
   lcd.setCursor(5, 0);
   lcd.print(heure);
   lcd.setCursor(12, 0);
   lcd.print("Pres");
   lcd.setCursor(0, 1);
   lcd.print(round(temp));
   lcd.setCursor(3, 1);
   if(modeConfort) lcd.print("C confo");
   else lcd.print("C  eco");
   lcd.setCursor(12, 1);
   lcd.print(pres);     
   break; 
    
   // Ecran réglage heure bascule du mode confort :
   case 3:
      lcd.print("Temp");
      lcd.setCursor(5, 0);
      lcd.print(heureConf);
      lcd.setCursor(12, 0);
      lcd.print("Pres");
      lcd.setCursor(0, 1);
      lcd.print(tempConf);
      lcd.setCursor(3, 1);
      lcd.print("C >conf<");
      lcd.setCursor(12, 1);
      lcd.print(presConf);
      break;
      
   // Ecran réglage température & pression du mode confort :
   case 4:
      lcd.print("Temp");
      lcd.setCursor(5, 0);
      lcd.print(heureConf);
      lcd.setCursor(12, 0);
      lcd.print("Pres");
      lcd.setCursor(0, 1);
      lcd.print(tempConf);
      lcd.setCursor(3, 1);
      lcd.print("C< conf >");
      lcd.setCursor(12, 1);
      lcd.print(presConf);
      break;
      
   // Ecran réglage heure bacule du mode économique :
   case 5:
      lcd.print("Temp");
      lcd.setCursor(5, 0);
      lcd.print(heureEco);
      lcd.setCursor(12, 0);
      lcd.print("Pres");
      lcd.setCursor(0, 1);
      lcd.print(tempEco);
      lcd.setCursor(3, 1);
      lcd.print("C >eco<");
      lcd.setCursor(12, 1);
      lcd.print(presEco);
      break;

   // Ecran réglage température & pression du mode économique :
   case 6:
      lcd.print("Temp");
      lcd.setCursor(5, 0);
      lcd.print(heureEco);
      lcd.setCursor(12, 0);
      lcd.print("Pres");
      lcd.setCursor(0, 1);
      lcd.print(tempEco);
      lcd.setCursor(3, 1);
      lcd.print("C< eco  >");
      lcd.setCursor(12, 1);
      lcd.print(presEco);
      break;
      
   // Ecran réglage de l'heure  :
   case 7:      
      lcd.print(heure);    
      lcd.setCursor(4, 1);
      lcd.print("->time<-");      
      break; 
      
   } // Fin switch
 
} // Fin de la fonction.



// ---------------------------------------------------------------
// formatHeure
// ---------------------------------------------------------------

String formatHeure(byte h, byte mn){
  // Fonction qui prend en argument les heures et les minutes et
  // qui renvoie l'heure au format HH:MM

  // Variables locales :
  // -------------------
  String hh = String();
  String mm = String();
  
  
  // Routine de la fonction :
  // ------------------------    
  if( h < 10 ) hh = "0" + String(h);
  else hh = String(h);
  
  if(mn < 10) mm = "0" + String(mn);
  else mm = String(mn);
  
  String heure = hh + ":" + mm;
  
  return heure;
 
} // Fin de la fonction.

void ventilateur(){

int moteur = analogRead(PIN_PWM);

 if (modeConfort == true){
 
 deltaPresIdeale = presConf - 1013.25; // 1 013,25 Pression atmosphérique moyenne mesuré à Paris 
 deltaPresMesurer = pres - 1013.25; // 1 013,25 Pression atmosphérique moyenne mesuré à Paris

 }
 
 if (modeConfort == false){
 
 deltaPresIdeale = presEco - 1013.25; // 1 013,25 Pression atmosphérique moyenne mesuré à Paris 
 deltaPresMesurer = pres - 1013.25; // 1 013,25 Pression atmosphérique moyenne mesuré à Paris

 }
 
   if (deltaPresMesurer < deltaPresIdeale * 1) { moteur = 255 * 0;}

   if (deltaPresMesurer < deltaPresIdeale * 0.75){ moteur = 255 * 0.33;}

   if (deltaPresMesurer < deltaPresIdeale * 0.5){ moteur = 255 * 0.66;}

   if (deltaPresMesurer < deltaPresIdeale * 0.25){ moteur = 255 * 1;}
 
analogWrite(PIN_PWM , moteur);

}



