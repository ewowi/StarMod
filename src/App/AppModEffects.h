/*
   @title     StarMod
   @file      AppModEffects.h
   @date      20240228
   @repo      https://github.com/ewowi/StarMod
   @Authors   https://github.com/ewowi/StarMod/commits/main
   @Copyright © 2024 Github StarMod Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
   @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
*/

#include "SysModule.h"

#include "I2SClocklessLedDriver.h"

#include "FastLED.h" // still used for helper methods

#include "AppFixture.h"
#include "AppEffects.h"

// #ifdef STARMOD_USERMOD_E131
//   #include "../User/UserModE131.h"
// #endif

// #define FASTLED_RGBW

//https://www.partsnotincluded.com/fastled-rgbw-neopixels-sk6812/
inline unsigned16 getRGBWsize(unsigned16 nleds){
	unsigned16 nbytes = nleds * 4;
	if(nbytes % 3 > 0) return nbytes / 3 + 1;
	else return nbytes / 3;
}

//https://github.com/FastLED/FastLED/blob/master/examples/DemoReel100/DemoReel100.ino
//https://blog.ja-ke.tech/2019/06/02/neopixel-performance.html

class AppModEffects:public SysModule {

public:
  bool newFrame = false; //for other modules (DDP)

  unsigned16 fps = 60;
  unsigned long lastMappingMillis = 0;
  Effects effects;

  Fixture fixture = Fixture();

  I2SClocklessLedDriver driver;
  boolean driverInit = false;

  AppModEffects() :SysModule("Effects") {
  };

  void setup() {
    SysModule::setup();

    parentVar = ui->initAppMod(parentVar, name, 1200);

    JsonObject currentVar;

    JsonObject tableVar = ui->initTable(parentVar, "fxTbl", nullptr, false, [this](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_UIFun:
        ui->setLabel(var, "Effects");
        ui->setComment(var, "List of effects");
        return true;
      case f_AddRow: {
        rowNr = fixture.ledsList.size();
        USER_PRINTF("chFun addRow %s[%d]\n", mdl->varID(var), rowNr);

        web->getResponseObject()["addRow"]["rowNr"] = rowNr;

        if (rowNr >= fixture.ledsList.size())
          fixture.ledsList.push_back(new Leds(fixture));
        return true;
      }
      case f_DelRow: {
        USER_PRINTF("chFun delrow %s[%d]\n", mdl->varID(var), rowNr);
        //tbd: fade to black
        if (rowNr <fixture.ledsList.size()) {
          Leds *leds = fixture.ledsList[rowNr];
          fixture.ledsList.erase(fixture.ledsList.begin() + rowNr); //remove from vector
          delete leds; //remove leds itself
        }
        return true;
      }
      default: return false;
    }});

    currentVar = ui->initSelect(tableVar, "fx", 0, false, [this](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_ValueFun:
        for (unsigned8 rowNr = 0; rowNr < fixture.ledsList.size(); rowNr++)
          mdl->setValue(var, fixture.ledsList[rowNr]->fx, rowNr);
        return true;
      case f_UIFun: {
        ui->setLabel(var, "Effect");
        ui->setComment(var, "Effect to show");
        JsonArray options = ui->setOptions(var);
        for (Effect *effect:effects.effects) {
          options.add(effect->name());
        }
        return true;
      }
      case f_ChangeFun:

        if (rowNr == UINT8_MAX) rowNr = 0; // in case fx without a rowNr

        //create a new leds instance if a new row is created
        if (rowNr >= fixture.ledsList.size()) {
          USER_PRINTF("ledslist fx[%d] changeFun %d %s\n", rowNr, fixture.ledsList.size(), mdl->findVar("fx")["value"].as<String>().c_str());
          fixture.ledsList.push_back(new Leds(fixture));
        }
        if (rowNr < fixture.ledsList.size())
          effects.setEffect(*fixture.ledsList[rowNr], var, rowNr);
        return true;
      default: return false;
    }});
    currentVar["stage"] = true;

    currentVar = ui->initSelect(tableVar, "pro", 2, false, [this](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_ValueFun:
        for (unsigned8 rowNr = 0; rowNr < fixture.ledsList.size(); rowNr++)
          mdl->setValue(var, fixture.ledsList[rowNr]->projectionNr, rowNr);
        return true;
      case f_UIFun: {
        ui->setLabel(var, "Projection");
        ui->setComment(var, "How to project fx");
        JsonArray options = ui->setOptions(var); // see enum Projections in AppFixture.h and keep the same order !
        options.add("Default");
        options.add("Multiply");
        options.add("Rotate");
        options.add("Distance from point");
        options.add("None");
        options.add("Random");
        options.add("Fun");
        options.add("Mirror WIP");
        options.add("Reverse WIP");
        options.add("Kaleidoscope WIP");
        return true;
      }
      case f_ChangeFun:

        if (rowNr == UINT8_MAX) rowNr = 0; // in case fx without a rowNr

        if (rowNr < fixture.ledsList.size()) {
          fixture.ledsList[rowNr]->doMap = true;

          unsigned8 proValue = mdl->getValue(var, rowNr);
          fixture.ledsList[rowNr]->projectionNr = proValue;

          mdl->varPreDetails(var, rowNr); //set all positive var N orders to negative
          if (proValue == p_DistanceFromPoint) {
            ui->initCoord3D(var, "proPoint", Coord3D{8,8,8}, 0, NUM_LEDS_Max);
            // , [](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
            //   case f_ValueFun:
            //     mdl->setValue(var, eff->fixture.size);
            //     return true;
            //   case f_UIFun:
            //     ui->setLabel(var, "Size");
            //     return true;
            //   default: return false;
            // }});
          }
          if (proValue == p_Multiply) {
            ui->initCoord3D(var, "proSplit", Coord3D{2,2,1}, 0, 10, false, [this](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
              case f_ChangeFun:
                fixture.ledsList[rowNr]->doMap = true;
                fixture.doMap = true;
                return true;
              default: return false;
            }});
          }
          if (proValue == p_Rotate) {
            ui->initCoord3D(var, "proCenter", Coord3D{8,8,8}, 0, NUM_LEDS_Max);
            ui->initSlider(var, "proSpeed", 1, 0, 60);
          }
          mdl->varPostDetails(var, rowNr);

          USER_PRINTF("chFun pro[%d] <- %d (%d)\n", rowNr, proValue, fixture.ledsList.size());

          fixture.doMap = true;
        }
        return true;
      default: return false;
    }});
    currentVar["stage"] = true;

    ui->initCoord3D(tableVar, "fxStart", {0,0,0}, 0, NUM_LEDS_Max, false, [this](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_ValueFun:
        for (unsigned8 rowNr = 0; rowNr < fixture.ledsList.size(); rowNr++) {
          USER_PRINTF("fxStart[%d] valueFun %d,%d,%d\n", rowNr, fixture.ledsList[rowNr]->startPos.x, fixture.ledsList[rowNr]->startPos.y, fixture.ledsList[rowNr]->startPos.z);
          mdl->setValue(var, fixture.ledsList[rowNr]->startPos, rowNr);
        }
        return true;
      case f_UIFun:
        ui->setLabel(var, "Start");
        ui->setComment(var, "In pixels");
        return true;
      case f_ChangeFun:
        if (rowNr < fixture.ledsList.size()) {
          fixture.ledsList[rowNr]->startPos = mdl->getValue(var, rowNr).as<Coord3D>();

          USER_PRINTF("fxStart[%d] chFun %d,%d,%d\n", rowNr, fixture.ledsList[rowNr]->startPos.x, fixture.ledsList[rowNr]->startPos.y, fixture.ledsList[rowNr]->startPos.z);

          fixture.ledsList[rowNr]->fadeToBlackBy();
          fixture.ledsList[rowNr]->doMap = true;
          fixture.doMap = true;
        }
        else {
          USER_PRINTF("fxStart[%d] chfun rownr not in range > %d\n", rowNr, fixture.ledsList.size());
        }
        return true;
      default: return false;
    }});

    ui->initCoord3D(tableVar, "fxEnd", {8,8,0}, 0, NUM_LEDS_Max, false, [this](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_ValueFun:
        for (unsigned8 rowNr = 0; rowNr < fixture.ledsList.size(); rowNr++) {
          USER_PRINTF("fxEnd[%d] valueFun %d,%d,%d\n", rowNr, fixture.ledsList[rowNr]->endPos.x, fixture.ledsList[rowNr]->endPos.y, fixture.ledsList[rowNr]->endPos.z);
          mdl->setValue(var, fixture.ledsList[rowNr]->endPos, rowNr);
        }
        return true;
      case f_UIFun:
        ui->setLabel(var, "End");
        ui->setComment(var, "In pixels");
        return true;
      case f_ChangeFun:
        if (rowNr < fixture.ledsList.size()) {
          fixture.ledsList[rowNr]->endPos = mdl->getValue(var, rowNr).as<Coord3D>();

          USER_PRINTF("fxEnd[%d] chFun %d,%d,%d\n", rowNr, fixture.ledsList[rowNr]->endPos.x, fixture.ledsList[rowNr]->endPos.y, fixture.ledsList[rowNr]->endPos.z);

          fixture.ledsList[rowNr]->fadeToBlackBy();
          fixture.ledsList[rowNr]->doMap = true;
          fixture.doMap = true;
        }
        else {
          USER_PRINTF("fxEnd[%d] chfun rownr not in range > %d\n", rowNr, fixture.ledsList.size());
        }
        return true;
      default: return false;
    }});

    ui->initText(tableVar, "fxSize", nullptr, 32, true, [this](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_ValueFun: {
        // for (std::vector<Leds *>::iterator leds=fixture.ledsList.begin(); leds!=fixture.ledsList.end(); ++leds) {
        unsigned8 rowNr = 0;
        for (Leds *leds: fixture.ledsList) {
          char message[32];
          print->fFormat(message, sizeof(message)-1, "%d x %d x %d = %d", leds->size.x, leds->size.y, leds->size.z, leds->nrOfLeds);
          USER_PRINTF("valueFun fxSize[%d](of %d) = %s\n", rowNr, fixture.ledsList.size(), message);
          mdl->setValue(var, JsonString(message, JsonString::Copied), rowNr); //rowNr
          rowNr++;
        }
        return true; }
      case f_UIFun:
        ui->setLabel(var, "Size");
        return true;
      default: return false;
    }});

    // ui->initSelect(parentVar, "fxLayout", 0, false, [](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
    //   case f_UIFun: {
    //     ui->setLabel(var, "Layout");
    //     ui->setComment(var, "WIP");
    //     JsonArray options = ui->setOptions(var);
    //     options.add("□"); //0
    //     options.add("="); //1
    //     options.add("||"); //2
    //     options.add("+"); //3
    //     return true;
    //   }
    //   default: return false;
    // }}); //fxLayout

    ui->initSlider(parentVar, "Blending", fixture.globalBlend, 0, 255, false, [this](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_ChangeFun:
        fixture.globalBlend = var["value"];
        return true;
      default: return false;
    }});

    #ifdef STARMOD_USERMOD_E131
      // if (e131mod->isEnabled) {
          e131mod->patchChannel(0, "bri", 255); //should be 256??
          e131mod->patchChannel(1, "fx", effects.effects.size());
          e131mod->patchChannel(2, "pal", 8); //tbd: calculate nr of palettes (from select)
          // //add these temporary to test remote changing of this values do not crash the system
          // e131mod->patchChannel(3, "pro", Projections::count);
          // e131mod->patchChannel(4, "fixture", 5); //assuming 5!!!

          // ui->stageVarChanged = true;
          // //rebuild the table
          for (JsonObject childVar: mdl->varChildren("e131Tbl"))
            ui->callVarFun(childVar, UINT8_MAX, f_ValueFun);

      // }
      // else
      //   USER_PRINTF("Leds e131 not enabled\n");
    #endif

    effects.setup();

  }

  void loop() {
    // SysModule::loop();

    //set new frame
    if (millis() - frameMillis >= 1000.0/fps) {
      frameMillis = millis();

      newFrame = true;

      //for each programmed effect
      //  run the next frame of the effect
      // vector iteration on classes is faster!!! (22 vs 30 fps !!!!)
      // for (std::vector<Leds *>::iterator leds=fixture.ledsList.begin(); leds!=fixture.ledsList.end(); ++leds) {
      unsigned8 rowNr = 0;
      for (Leds *leds: fixture.ledsList) {
        if (!leds->doMap) { // don't run effect while remapping
          // USER_PRINTF(" %d %d,%d,%d - %d,%d,%d (%d,%d,%d)", leds->fx, leds->startPos.x, leds->startPos.y, leds->startPos.z, leds->endPos.x, leds->endPos.y, leds->endPos.z, leds->size.x, leds->size.y, leds->size.z );
          mdl->contextRowNr = rowNr++;
          effects.loop(*leds);
          mdl->contextRowNr = UINT8_MAX;
        }
      }

      if(driverInit) driver.showPixels();

      frameCounter++;
    }
    else {
      newFrame = false;
    }

    JsonObject var = mdl->findVar("System");
    if (!var["canvasData"].isNull()) {
      const char * canvasData = var["canvasData"]; //0 - 494 - 140,150,0
      USER_PRINTF("AppModEffects loop canvasData %s\n", canvasData);

      //currently only leds[0] supported
      if (fixture.ledsList.size()) {
        fixture.ledsList[0]->fadeToBlackBy();

        char * token = strtok((char *)canvasData, ":");
        bool isStart = strcmp(token, "start") == 0;

        Coord3D *startOrEndPos = isStart? &fixture.ledsList[0]->startPos: &fixture.ledsList[0]->endPos;

        token = strtok(NULL, ",");
        if (token != NULL) startOrEndPos->x = atoi(token) / 10; else startOrEndPos->x = 0; //should never happen
        token = strtok(NULL, ",");
        if (token != NULL) startOrEndPos->y = atoi(token) / 10; else startOrEndPos->y = 0;
        token = strtok(NULL, ",");
        if (token != NULL) startOrEndPos->z = atoi(token) / 10; else startOrEndPos->z = 0;

        mdl->setValue(isStart?"fxStart":"fxEnd", *startOrEndPos, 0); //assuming row 0 for the moment

        var.remove("canvasData"); //convasdata has been processed
        fixture.ledsList[0]->doMap = true; //recalc projection
        fixture.doMap = true;
      }
    }

    //update projection
    if (millis() - lastMappingMillis >= 1000 && fixture.doMap) { //not more then once per second (for E131)
      lastMappingMillis = millis();
      fixture.projectAndMap();

      //https://github.com/FastLED/FastLED/wiki/Multiple-Controller-Examples

      //connect allocated Pins to gpio

      if (fixture.doAllocPins) {
        unsigned pinNr = 0;

        for (PinObject pinObject:pins->pinObjects) {

          if (pins->isOwner(pinNr, "Leds")) { //if pin owned by leds, (assigned in projectAndMap)
            //dirty trick to decode nrOfLedsPerPin
            char * after = strtok((char *)pinObject.details, "-");
            if (after != NULL ) {
              char * before;
              before = after;
              after = strtok(NULL, " ");

              unsigned16 startLed = atoi(before);
              unsigned16 nrOfLeds = atoi(after) - atoi(before) + 1;
              USER_PRINTF("driver.initled new %d: %d-%d\n", pinNr, startLed, nrOfLeds);

              int pins[1] = { pinNr };
              driver.initled((uint8_t*) fixture.ledsP, pins, 1, (int) nrOfLeds, ORDER_GRB);

              driverInit = true;

            } //if led range in details (- in details e.g. 0-1023)
          } //if pin owned by leds
          pinNr++;
        } // for pins
        fixture.doAllocPins = false;
      }
    }
  } //loop

  void loop1s() {
    mdl->setUIValueV("realFps", "%lu /s", frameCounter);
    frameCounter = 0;
  }

private:
  unsigned long frameMillis = 0;
  unsigned long frameCounter = 0;

};

static AppModEffects *eff;