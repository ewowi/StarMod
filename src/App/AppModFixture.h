/*
   @title     StarMod
   @file      AppModFixture.h
   @date      20240228
   @repo      https://github.com/ewowi/StarMod
   @Authors   https://github.com/ewowi/StarMod/commits/main
   @Copyright © 2024 Github StarMod Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
   @license   For non GPL-v3 usage, commercial licenses must be purchased. Contact moonmodules@icloud.com
*/

class AppModFixture:public SysModule {

public:

  AppModFixture() :SysModule("Fixture") {};

  void setup() {
    SysModule::setup();

    parentVar = ui->initAppMod(parentVar, name, 1100);

    JsonObject currentVar = ui->initCheckBox(parentVar, "on", true, false, [](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_UIFun:
        ui->setLabel(var, "On");
        return true;
      case f_ChangeFun:
        ui->callVarFun("bri", UINT8_MAX, f_ChangeFun); //set FastLed brightness
        return true;
      default: return false;
    }});
    currentVar["stage"] = true;

    //logarithmic slider (10)
    currentVar = ui->initSlider(parentVar, "bri", 10, 0, 255, false, [](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_UIFun:
        ui->setLabel(var, "Brightness");
        return true;
      case f_ChangeFun: {
        unsigned8 bri = var["value"];

        unsigned8 result = mdl->getValue("on").as<bool>()?linearToLogarithm(var, bri):0;

        // TODO: driver.setBrightness(result);

        USER_PRINTF("Set Brightness to %d -> b:%d r:%d\n", var["value"].as<int>(), bri, result);
        return true; }
      default: return false; 
    }});
    currentVar["log"] = true; //logarithmic
    currentVar["stage"] = true; //these values override model.json???

    ui->initCanvas(parentVar, "pview", UINT16_MAX, false, [this](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_UIFun:
        ui->setLabel(var, "Preview");
        ui->setComment(var, "Shows the fixture");
        // ui->setComment(var, "Click to enlarge");
        return true;
      case f_LoopFun: {
        var["interval"] =  max(eff->fixture.nrOfLeds * web->ws->count()/200, 16U)*10; //interval in ms * 10, not too fast //from cs to ms

        web->sendDataWs([this](AsyncWebSocketMessageBuffer * wsBuf) {
          byte* buffer;

          buffer = wsBuf->get();

          // send leds preview to clients
          for (size_t i = 0; i < eff->fixture.nrOfLeds; i++)
          {
            buffer[i*3+5] = eff->fixture.ledsP[i].red;
            buffer[i*3+5+1] = eff->fixture.ledsP[i].green;
            buffer[i*3+5+2] = eff->fixture.ledsP[i].blue;
          }
          //new values
          buffer[0] = 1; //userFun id
          buffer[1] = eff->fixture.head.x;
          buffer[2] = eff->fixture.head.y;
          buffer[3] = eff->fixture.head.y;

        }, eff->fixture.nrOfLeds * 3 + 5, true);
        return true;
      }
      default: return false;
    }});

    ui->initSelect(parentVar, "fixture", eff->fixture.fixtureNr, false ,[](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_UIFun:
      {
        // ui->setComment(var, "Fixture to display effect on");
        JsonArray options = ui->setOptions(var);
        files->dirToJson(options, true, "F_"); //only files containing F(ixture), alphabetically

        // ui needs to load the file also initially
        char fileName[32] = "";
        if (files->seqNrToName(fileName, var["value"])) {
          web->addResponse("pview", "file", JsonString(fileName, JsonString::Copied));
        }
        return true;
      }
      case f_ChangeFun:
      {
        eff->fixture.fixtureNr = var["value"];
        eff->fixture.doMap = true;
        eff->fixture.doAllocPins = true;

        //remap all leds
        // for (std::vector<Leds *>::iterator leds=eff->fixture.ledsList.begin(); leds!=eff->fixture.ledsList.end(); ++leds) {
        for (Leds *leds: eff->fixture.ledsList) {
          leds->doMap = true;
        }

        char fileName[32] = "";
        if (files->seqNrToName(fileName, eff->fixture.fixtureNr)) {
          //send to pview a message to get file filename
          web->addResponse("pview", "file", JsonString(fileName, JsonString::Copied));
        }
        return true;
      }
      default: return false; 
    }}); //fixture

    ui->initCoord3D(parentVar, "fixSize", eff->fixture.size, 0, NUM_LEDS_Max, true, [](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_ValueFun:
        mdl->setValue(var, eff->fixture.size);
        return true;
      case f_UIFun:
        ui->setLabel(var, "Size");
        return true;
      default: return false;
    }});

    ui->initNumber(parentVar, "fixCount", eff->fixture.nrOfLeds, 0, UINT16_MAX, true, [](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_ValueFun:
        mdl->setValue(var, eff->fixture.nrOfLeds);
        return true;
      case f_UIFun:
        ui->setLabel(var, "Count");
        web->addResponseV(var["id"], "comment", "Max %d", NUM_LEDS_Max);
        return true;
      default: return false;
    }});

    ui->initNumber(parentVar, "fps", eff->fps, 1, 999, false , [](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_UIFun:
        ui->setComment(var, "Frames per second");
        return true;
      case f_ChangeFun:
        eff->fps = var["value"];
        return true;
      default: return false; 
    }});

    ui->initText(parentVar, "realFps", nullptr, 10, true, [](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
      case f_UIFun:
        web->addResponseV(var["id"], "comment", "f(%d leds)", eff->fixture.nrOfLeds);
        return true;
      default: return false;
    }});

    #ifdef STARMOD_USERMOD_WLEDAUDIO
      ui->initCheckBox(parentVar, "mHead", false, false, [](JsonObject var, unsigned8 rowNr, unsigned8 funType) { switch (funType) { //varFun
        case f_UIFun:
          ui->setLabel(var, "Moving heads");
          ui->setComment(var, "Move on GEQ");
          return true;
        case f_ChangeFun:
          if (!var["value"])
            eff->fixture.head = {0,0,0};
          return true;
        default: return false; 
      }});
    #endif

  }
};

static AppModFixture *fix;