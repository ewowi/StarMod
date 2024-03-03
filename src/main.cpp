/*
   @title     StarMod
   @file      main.cpp
   @date      20240114
   @repo      https://github.com/ewowi/StarMod
   @Authors   https://github.com/ewowi/StarMod/commits/main
   @Copyright © 2024 Github StarMod Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
   @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
*/

// remove latest commit
// git reset --hard HEAD^
// git push origin -f

#warning ******************************************************************************************************************************
#warning ********** STARMOD IS LICENSED UNDER GPL-V3. BY INSTALLING STARMOD YOU IMPLICITLY ACCEPT THE TERMS AND CONDITIONS  ********
#warning ******************************************************************************************************************************

#include "SysModule.h"
#include "SysModules.h"
#include "Sys/SysModPrint.h"
#include "Sys/SysModWeb.h"
#include "Sys/SysModUI.h"
#include "Sys/SysModSystem.h"
#include "Sys/SysModFiles.h"
#include "Sys/SysModModel.h"
#include "Sys/SysModNetwork.h"
#include "Sys/SysModPins.h"
#include "User/UserModInstances.h"
#include "User/UserModMDNS.h"
#ifdef STARMOD_APPMOD_LEDS
  #include "App/AppModEffects.h"
  #include "App/AppModFixture.h"
  #include "App/AppModFixtureGen.h"
  #ifdef STARMOD_USERMOD_ARTNET
    #include "User/UserModArtNet.h"
  #endif
  #ifdef STARMOD_USERMOD_DDP
    #include "User/UserModDDP.h"
  #endif
  #ifdef USERMOD_HUB75
    #include "User/UserModHub75.h"
  #endif
#endif
#ifdef STARMOD_USERMOD_E131
  #include "User/UserModE131.h"
#endif
#ifdef STARMOD_USERMOD_HA
  #include "User/UserModHA.h"
#endif
#ifdef STARMOD_USERMOD_WLEDAUDIO
  #include "User/UserModWLEDAudio.h"
#endif
#include "Sys/SysModAI.h"


//setup all modules
void setup() {
  // pinMode(LED_BUILTIN, OUTPUT);
  // digitalWrite(LED_BUILTIN, HIGH);

  mdls = new SysModules();
  
  ai = new SysModAI();
  print = new SysModPrint();
  files = new SysModFiles();
  mdl = new SysModModel();
  net = new SysModNetwork();
  web = new SysModWeb();
  ui = new SysModUI();
  sys = new SysModSystem();
  pins = new SysModPins();
  instances = new UserModInstances();
  mdns = new UserModMDNS();
  #ifdef STARMOD_APPMOD_LEDS
    eff= new AppModEffects();
    fix = new AppModFixture();
    lfg = new AppModFixtureGen();
    #ifdef STARMOD_USERMOD_ARTNET
      artnetmod = new UserModArtNet();
    #endif
    #ifdef STARMOD_USERMOD_DDP
      ddpmod = new UserModDDP();
    #endif
  #endif
  #ifdef STARMOD_USERMOD_E131
    e131mod = new UserModE131();
  #endif
  #ifdef STARMOD_USERMOD_HA
    hamod = new UserModHA();
  #endif
  #ifdef STARMOD_USERMOD_WLEDAUDIO
    wledAudioMod = new UserModWLEDAudio();
  #endif
  #ifdef USERMOD_HUB75
    hub75Mod = new UserModHub75();
  #endif

  //Reorder with care! If changed make sure mdlEnabled.chFun executes var["value"].to<JsonArray>(); and saveModel! 
  //Default: add below, not in between
  mdls->add(ai);
  #ifdef STARMOD_APPMOD_LEDS
    mdls->add(fix);
    mdls->add(eff);
    mdls->add(lfg);
  #endif
  mdls->add(files);
  mdls->add(sys);
  mdls->add(pins);
  mdls->add(print);
  mdls->add(web);
  mdls->add(net);
  #ifdef STARMOD_APPMOD_LEDS
    #ifdef STARMOD_USERMOD_DDP
      mdls->add(ddpmod);
    #endif
    #ifdef STARMOD_USERMOD_ARTNET
      mdls->add(artnetmod);
    #endif
  #endif
  #ifdef STARMOD_USERMOD_E131
    mdls->add(e131mod);
  #endif
  #ifdef STARMOD_USERMOD_HA
    mdls->add(hamod); //no ui
  #endif
  mdls->add(mdl);
  mdls->add(ui);
  #ifdef STARMOD_USERMOD_WLEDAUDIO
    mdls->add(wledAudioMod); //no ui
  #endif
  mdls->add(mdns); //no ui
  #ifdef USERMOD_HUB75
    mdls->add(hub75Mod);
  #endif
  mdls->add(instances);

  //do not add mdls itself as it does setup and loop for itself!!! (it is the orchestrator)
  mdls->setup();

  // digitalWrite(LED_BUILTIN, LOW);
}

//loop all modules
void loop() {
  mdls->loop();

  // static bool onoff = false;
  // onoff = !onoff;
  // digitalWrite(LED_BUILTIN, onoff? HIGH:LOW);
}