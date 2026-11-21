static int p4ChannelBank=0;
static lv_obj_t *p4RadioStateLabel=nullptr, *p4RadioDetailLabel=nullptr;
static lv_obj_t *p4ChannelBars[7], *p4ChannelValues[7], *p4ChannelNumbers[7];
static constexpr int p4BankFirst=-22600, p4BankLast=-22601, p4ModeBase=-22610;
static constexpr int p4OpenProfiles=-22619, p4ProfileBase=-22620;
static constexpr const char* p4ProfileNames[]={"Dragon Radar","Axon Watch","ALPR Watch","Sky Watch"};

static lv_obj_t* p4Centered(const char* text,int y,int width,const lv_font_t* font){
  auto *obj=label(root,text,(360-width)/2,y,width,font);
  lv_obj_set_style_text_align(obj,LV_TEXT_ALIGN_CENTER,0);
  return obj;
}

static void p4RefreshRadioPage(bool force){
  if(page!=P4Channels && page!=P4Beacons && page!=P4Modes && page!=P4Profiles && page!=P4BleLab)return;
  static uint32_t lastDraw=0;
  const uint32_t now=millis();
  if(!force && now-lastDraw<350)return;
  lastDraw=now;
  char text[144];
  if(page==P4Channels){
    uint16_t counts[14]={};int8_t strongest[14]={};uint32_t completed=0;
    const bool valid=p4RadarChannelSummary(counts,strongest,&completed);
    unsigned maximum=1;
    if(valid)for(uint16_t count:counts)if(count>maximum)maximum=count;
    for(int i=0;i<7;++i){
      const int channel=i+7*p4ChannelBank;
      snprintf(text,sizeof(text),"%02d",channel+1);setText(p4ChannelNumbers[i],text);
      lv_bar_set_value(p4ChannelBars[i],valid?int((uint32_t(counts[channel])*100)/maximum):0,LV_ANIM_OFF);
      if(!valid)strcpy(text,"--     --");
      else if(!counts[channel] || strongest[channel]==INT8_MIN)snprintf(text,sizeof(text),"%u     --",unsigned(counts[channel]));
      else snprintf(text,sizeof(text),"%u     %d",unsigned(counts[channel]),int(strongest[channel]));
      setText(p4ChannelValues[i],text);
    }
    if(valid)snprintf(text,sizeof(text),"%s / %lus ago",p4RadarStatus(),(unsigned long)((now-completed)/1000));
    else snprintf(text,sizeof(text),"%s / awaiting survey",p4RadarStatus());
    setText(p4RadioStateLabel,text);
  }else if(page==P4Modes){
    static const char* names[]={"Wi-Fi + Bluetooth","Wi-Fi","Bluetooth"};
    const unsigned mode=p4RadarScanMode();
    snprintf(text,sizeof(text),"Selected: %s",mode<3?names[mode]:"Unknown");
    setText(p4RadioStateLabel,text);
  }else if(page==P4Profiles){
    const unsigned profile=unsigned(reconMode());
    snprintf(text,sizeof(text),"Selected: %s",profile<4?p4ProfileNames[profile]:"Dragon Radar");
    setText(p4RadioStateLabel,text);
  }else{
    snprintf(text,sizeof(text),"Bluetooth: %s",p4RadarBluetoothStatus());
    setText(p4RadioStateLabel,text);
    if(page==P4Beacons){
      snprintf(text,sizeof(text),"iBeacon     %lu\nEddystone     %lu\n\nListening windows     %lu",
          (unsigned long)p4RadarIbeaconReports(),(unsigned long)p4RadarEddystoneReports(),
          (unsigned long)p4RadarBleWindows());
    }else{
      snprintf(text,sizeof(text),"%lu advertisements / %lu windows",
          (unsigned long)p4RadarBleReports(),(unsigned long)p4RadarBleWindows());
    }
    setText(p4RadioDetailLabel,text);
  }
}

static bool p4BuildRadioPage(){
  if(page==P4Channels){
    title("Channel Visualizer","Evil-M5Project / Wi-Fi");
    label(root,"CH",68,88,29,F12);label(root,"ACCESS POINTS",101,88,101,F12);
    label(root,"APs   PEAK dBm",202,88,92,F12);
    for(int i=0;i<7;++i){
      const int y=108+18*i;
      p4ChannelNumbers[i]=label(root,"",69,y,27,F12);
      auto* bar=lv_bar_create(root);lv_obj_set_pos(bar,S(101),S(y+5));
      lv_obj_set_size(bar,S(91),S(7));
      lv_obj_set_style_bg_color(bar,color(uiColors().accent),LV_PART_INDICATOR);
      p4ChannelBars[i]=bar;
      p4ChannelValues[i]=label(root,"",203,y,85,F12);
    }
    p4RadioStateLabel=p4Centered("",239,264,F12);
    button(root,"CH 1-7",74,262,100,33,p4BankFirst);
    button(root,"CH 8-14",185,262,100,33,p4BankLast);
  }else if(page==P4Beacons){
    title("BLE BEACON SCAN","Evil-M5Project");
    p4RadioStateLabel=p4Centered("",100,244,F14);
    p4RadioDetailLabel=p4Centered("",141,238,F14);
    p4Centered("Received advertisements since startup",237,244,F12);
  }else if(page==P4Modes){
    title("SCAN MODES","Passive discovery");
    p4RadioStateLabel=p4Centered("",90,248,F14);
    const char* choices[]={"Wi-Fi + Bluetooth","Wi-Fi only","Bluetooth only"};
    for(int i=0;i<3;++i)button(root,choices[i],75,117+42*i,210,35,p4ModeBase-i);
    button(root,"Detector profile",75,250,210,36,p4OpenProfiles);
  }else if(page==P4Profiles){
    title("DETECTOR PROFILE","Passive observations");
    p4RadioStateLabel=p4Centered("",89,250,F12);
    for(unsigned i=0;i<4;++i){
      auto *b=button(root,p4ProfileNames[i],74,111+36*i,212,32,p4ProfileBase-int(i));
      if(!p4RadarProfileSupported(i))lv_obj_add_state(b,LV_STATE_DISABLED);
    }
    p4Centered("Sky Watch uses broadcast BLE. Raw Wi-Fi frames are unavailable.",261,238,F12);
  }else if(page==P4BleLab){
    title("BLE LAB","Passive discovery");
    p4RadioStateLabel=p4Centered("",94,248,F14);
    p4RadioDetailLabel=p4Centered("",124,248,F12);
    button(root,"Observed devices",75,164,210,41,int(p4ui::Action::BleContacts));
    button(root,"BLE Beacon Scan",75,215,210,41,int(p4ui::Action::Beacons));
  }else if(page==P4More){
    title("MORE");
    button(root,"Alerts",58,115,117,44,22);button(root,"Notebook",184,115,117,44,153);
    button(root,"Radio health",58,173,117,44,99);button(root,"Radar style",184,173,117,44,117);
    button(root,"Radio Activity",80,234,200,43,int(p4ui::Action::RadioActivity));
  }else if(page==P4Capabilities){
    title("RADIO CAPABILITIES","Waveshare P4 Knob");
    p4Centered("Wi-Fi: passive access-point surveys",106,235,F14);
    char ble[90];snprintf(ble,sizeof(ble),"Bluetooth: %s",p4RadarBluetoothStatus());
    p4Centered(ble,157,235,F14);
    p4Centered("Raw frame attacks, CSI, and ESP-NOW node mode are unavailable on this build.",210,225,F14);
  }else return false;
  p4RefreshRadioPage(true);
  return true;
}

static bool p4RadioAction(int id){
  if(page==P4Modes && id==p4OpenProfiles){go(P4Profiles);return true;}
  if(page==P4Profiles && id<=p4ProfileBase && id>=p4ProfileBase-3){
    const unsigned next=unsigned(p4ProfileBase-id);
    if(!p4RadarProfileSupported(next) || !reconSetMode(dragon::ReconMode(next)))
      p4Message("DETECTOR PROFILE","This receiver cannot use that profile right now.");
    p4RefreshRadioPage(true);return true;
  }
  if(page==P4Channels && (id==p4BankFirst || id==p4BankLast)){
    p4ChannelBank=id==p4BankLast; p4RefreshRadioPage(true);return true;
  }
  if(page==P4Modes && id<=p4ModeBase && id>=p4ModeBase-2){
    if(!p4RadarSetScanMode(unsigned(p4ModeBase-id)))
      p4Message("SCAN MODE","This receiver cannot use that mode right now.");
    p4RefreshRadioPage(true); return true;
  }
  return false;
}
