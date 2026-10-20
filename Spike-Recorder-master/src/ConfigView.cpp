#include "ConfigView.h"
#include "engine/RecordingManager.h"
#include "engine/BASSErrors.h"
#include "widgets/Painter.h"
#include "widgets/Color.h"

#include "widgets/PushButton.h"
#include "widgets/TextureGL.h"
#include "widgets/Application.h"
#include "widgets/BitmapFontGL.h"
#include "widgets/ErrorBox.h"
#include "widgets/Label.h"
#include "widgets/ErrorBox.h"
#define BOARD_WITH_JOYSTICK 5

#include "DropDownList.h"
#include "AudioView.h"
#include "ColorDropDownList.h"
#include "Log.h"
#include <bass.h>
#include <sstream>
#include <SDL.h>
#include <SDL_opengl.h>
#include "CalibrationWindow.h"
#include "widgets/HorizontalColorPicker.h"
#include "widgets/HorizontalNumberPicker.h"


namespace BackyardBrains {
namespace {
class USBConnectButton : public Widgets::PushButton {
public:
    USBConnectButton(Widgets::Widget *parent):Widgets::PushButton(parent),_connected(false) {}
    void setConnected(bool connected) {_connected=connected;}
private:
    bool _connected;
    void paintEvent() {
        glDisable(GL_TEXTURE_2D);
        Widgets::Painter::setColor(Widgets::Colors::button);
        Widgets::Painter::drawRoundedRect(rect(),4);
        glEnable(GL_TEXTURE_2D);
        Widgets::Painter::setColor(Widgets::Colors::white);
        Widgets::Application::font()->draw(_connected?"Disconnect":"Connect",width()/2,height()/2,Widgets::AlignCenter);
    }
};
}


ConfigView::ConfigView(RecordingManager &mngr, AudioView &audioView, Widget *parent) : Widget(parent), _manager(mngr), _audioView(audioView) {
    SetupScreen();
}

//----------------------------------- START OF CONFIG ----------------------------------------------
//--------------------------------------------------------------------------------------------------

void ConfigView::SetupScreen()
{
    
    
    weAreOnTouchScreen = Widgets::Application::getInstance()->areWeOnTouchscreen();


        Log::msg("Create close button...");
	Widgets::PushButton *closeButton = new Widgets::PushButton(this);
	closeButton->clicked.connect(this, &ConfigView::closePressed);
	closeButton->setNormalTex(Widgets::TextureGL::get("data/configcrossed.bmp"));
	closeButton->setHoverTex(Widgets::TextureGL::get("data/configcrossed.bmp"));

    Log::msg("Create top label...");
	Widgets::Label *topLabel = new Widgets::Label(this);
	topLabel->setText("Input & settings");
	topLabel->updateSize();
    Log::msg("Make colors");
	std::vector<Widgets::Color> colorSet(AudioView::COLORS, AudioView::COLORS+AudioView::COLOR_NUM);
	std::vector<Widgets::Color> arduinoColorSet(AudioView::ARDUINO_COLORS, AudioView::ARDUINO_COLORS+AudioView::COLOR_NUM);
	Log::msg("Access virtual devices");

    if(weAreOnTouchScreen)
    {
        _hclrs.resize(_manager.virtualDevices().size());
        _catchers.reserve(_hclrs.size());
    }
    else
    {
        _clrs.resize(_manager.virtualDevices().size());
        _catchers.reserve(_clrs.size());
    }


    Log::msg("Make box layout");
	Widgets::Widget *group = new Widgets::Widget(this);
	group->setSizeHint(Widgets::Size(500,2500));
	gvbox = new Widgets::BoxLayout(Widgets::Vertical, group);



    int chckBoxSize = 19;
    if(weAreOnTouchScreen)
    {
        chckBoxSize = 40;
    }


    //-------------- Mute check box ----------------------------------------

    Log::msg("Check file mode");
	if(!_manager.fileMode()) {
        Log::msg("Make mute checkbox");
		Widgets::BoxLayout *mutehbox = new Widgets::BoxLayout(Widgets::Horizontal);
		Widgets::Label *muteLabel = new Widgets::Label(group);
		muteLabel->setText("Mute Speakers:");
		muteLabel->updateSize();

		_muteCKBox = new Widgets::PushButton(group);
        Log::msg("Check state for mute checkbox");
		if(_manager.player().volume() == 0)
			_muteCKBox->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
		else
			_muteCKBox->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));


        _muteCKBox->setSizeHint(Widgets::Size(chckBoxSize,chckBoxSize));


		_muteCKBox->clicked.connect(this, &ConfigView::mutePressed);
        Log::msg("Add mute label to box");
		mutehbox->addWidget(muteLabel, Widgets::AlignVCenter);
		mutehbox->addSpacing(10);
		Log::msg("Add mute checkbox");
		mutehbox->addWidget(_muteCKBox, Widgets::AlignVCenter);
		mutehbox->addSpacing(50);
		Log::msg("Add layout for mute checkbox");
		gvbox->addLayout(mutehbox);
		gvbox->addSpacing(30);


        //-------------- Preset buttons ----------------------------------------
        Widgets::BoxLayout *presetBox = new Widgets::BoxLayout(Widgets::Horizontal);
        
        _presetPlantButton = new Widgets::PushButton(group);
        _presetPlantButton->clicked.connect(this, &ConfigView::presetPlantPressed);
        _presetPlantButton->setNormalTex(Widgets::TextureGL::get("data/preset-plant.bmp"));
        _presetPlantButton->setHoverTex(Widgets::TextureGL::get("data/preset-plant-high.bmp"));
        _presetPlantButton->setSizeHint(Widgets::Size(66,40));
        
        
        _presetNeuronButton = new Widgets::PushButton(group);
        _presetNeuronButton->clicked.connect(this, &ConfigView::presetNeuronPressed);
        _presetNeuronButton->setNormalTex(Widgets::TextureGL::get("data/preset-neuron.bmp"));
        _presetNeuronButton->setHoverTex(Widgets::TextureGL::get("data/preset-neuron-high.bmp"));
        _presetNeuronButton->setSizeHint(Widgets::Size(66,40));
        
        
        _presetEEGButton = new Widgets::PushButton(group);
        _presetEEGButton->clicked.connect(this, &ConfigView::presetEEGPressed);
        _presetEEGButton->setNormalTex(Widgets::TextureGL::get("data/preset-eeg.bmp"));
        _presetEEGButton->setHoverTex(Widgets::TextureGL::get("data/preset-eeg-high.bmp"));
        _presetEEGButton->setSizeHint(Widgets::Size(66,40));
        
        _presetECGButton = new Widgets::PushButton(group);
        _presetECGButton->clicked.connect(this, &ConfigView::presetECGPressed);
        _presetECGButton->setNormalTex(Widgets::TextureGL::get("data/preset-ecg.bmp"));
        _presetECGButton->setHoverTex(Widgets::TextureGL::get("data/preset-ecg-high.bmp"));
        _presetECGButton->setSizeHint(Widgets::Size(66,40));
        
        _presetEMGButton = new Widgets::PushButton(group);
        _presetEMGButton->clicked.connect(this, &ConfigView::presetEMGPressed);
        _presetEMGButton->setNormalTex(Widgets::TextureGL::get("data/preset-emg.bmp"));
        _presetEMGButton->setHoverTex(Widgets::TextureGL::get("data/preset-emg-high.bmp"));
        _presetEMGButton->setSizeHint(Widgets::Size(66,40));
        
        checkIfWeShouldHighlightPreset();
        
        presetBox->addSpacing(40);
        presetBox->addWidget(_presetECGButton);
        presetBox->addSpacing(20);
        presetBox->addWidget(_presetEEGButton);
        presetBox->addSpacing(20);
        presetBox->addWidget(_presetEMGButton);
        presetBox->addSpacing(20);
        presetBox->addWidget(_presetPlantButton);
        presetBox->addSpacing(20);
        presetBox->addWidget(_presetNeuronButton);

        gvbox->addLayout(presetBox);
        gvbox->addSpacing(30);
    }
    //---------- Calibrator code --------------------------------------
    /*
	if(!_manager.fileMode()) {
        Widgets::Label *calibrateMainLabel = new Widgets::Label(group);
		calibrateMainLabel->setText("Calibrate SpikeRecorder for current setup ");
		calibrateMainLabel->updateSize();

        Widgets::PushButton *calibrateButton = new Widgets::PushButton(group);
        calibrateButton->clicked.connect(this, &ConfigView::calibratePressed);
        calibrateButton->setNormalTex(Widgets::TextureGL::get("data/calibratebtn-normal.bmp"));
        calibrateButton->setHoverTex(Widgets::TextureGL::get("data/calibratebtn-high.bmp"));
        calibrateButton->setSize(Widgets::Size(80,26));

        Widgets::BoxLayout *hcalBox = new Widgets::BoxLayout(Widgets::Horizontal);
        hcalBox->addWidget(calibrateMainLabel);
        hcalBox->addSpacing(10);
        hcalBox->addWidget(calibrateButton);

        gvbox->addLayout(hcalBox);
        gvbox->addSpacing(40);
	}
*/
    //---------- High/Low pass filter --------------------------------------
    Log::msg("Check file mode 2");
    if(!_manager.fileMode()) {
        Log::msg("Add filter label");
        Widgets::Label *filterMainLabel = new Widgets::Label(group);
		filterMainLabel->setText("Set band-pass filter cutoff frequencies");
		filterMainLabel->updateSize();

        Log::msg("Create text inputs");

        int heightOfTextInput = 20;
        if(weAreOnTouchScreen)
        {
            heightOfTextInput= 30;
        }

        lowValueTI = new Widgets::TextInput(group, 50, heightOfTextInput);
        lowValueTI->textEditingEnded.connect(this, &ConfigView::lowFilterTIValueChanged);
        highValueTI = new Widgets::TextInput(group, 50, heightOfTextInput);
        highValueTI->textEditingEnded.connect(this, &ConfigView::highFilterTIValueChanged);

        Log::msg("Create range selector");
        int heightOfRangeSelector = 40;
        if(weAreOnTouchScreen)
        {
            heightOfRangeSelector = 80;
        }
        rangeSelector = new Widgets::RangeSelector(group, heightOfRangeSelector);
        rangeSelector->setRange(0,_manager.sampleRate()/2);
        Log::msg("Set parameters on range selector");
        highValueTI->setInt(_manager.lowCornerFrequency());
        lowValueTI->setInt(_manager.highCornerFrequency());
        Log::msg("Connect range selector");
        rangeSelector->lowValueChanged.connect(this, &ConfigView::lowFilterValueChanged);
        rangeSelector->highValueChanged.connect(this, &ConfigView::highFilterValueChanged);
        Log::msg("Update values range selector");
        rangeSelector->initHighAndLow( _manager.lowCornerFrequency(), _manager.highCornerFrequency());


        Log::msg("Create label for range boxes");
        Widgets::BoxLayout *labelsBox = new Widgets::BoxLayout(Widgets::Horizontal);

        Widgets::Label *lowFilterLabel = new Widgets::Label(group);
		lowFilterLabel->setText("Low");
		lowFilterLabel->updateSize();

        Widgets::Label *highFilterLabel = new Widgets::Label(group);
		highFilterLabel->setText("High");
		highFilterLabel->updateSize();
        Log::msg("Add filter labels to label boxes");
        labelsBox->addSpacing(8);
        labelsBox->addWidget(lowFilterLabel, Widgets::AlignBottom);
		labelsBox->addSpacing(415);
		labelsBox->addWidget(highFilterLabel, Widgets::AlignBottom);
        Log::msg("Add layout label box");
        gvbox->addLayout(labelsBox);

        Widgets::BoxLayout *valuesBox = new Widgets::BoxLayout(Widgets::Horizontal);
        Log::msg("Add components to value box");
		valuesBox->addWidget(lowValueTI);
		valuesBox->addSpacing(40);
		valuesBox->addWidget(filterMainLabel);
		valuesBox->addSpacing(40);
		valuesBox->addWidget(highValueTI);
		Log::msg("Add components ro gvbox");
		gvbox->addLayout(valuesBox);
        gvbox->addSpacing(3);
        gvbox->addWidget(rangeSelector);
		gvbox->addSpacing(20);
    }




    //----------- 50Hz/60Hz noise filter ------------------------------------

    Log::msg("Check file mode 3");
    if(!_manager.fileMode()) {
        Log::msg("Create filterbox");
		Widgets::BoxLayout *filterhbox = new Widgets::BoxLayout(Widgets::Horizontal);
        Log::msg("Create filter label for notch");
		Widgets::Label *filterLabel = new Widgets::Label(group);
		filterLabel->setText("Attenuate frequency (Notch filter):");
		filterLabel->updateSize();
        Log::msg("Create 50Hz label");
		Widgets::Label *fiftyHzLabel = new Widgets::Label(group);
		fiftyHzLabel->setText("50Hz");
		fiftyHzLabel->updateSize();
        Log::msg("Create 60Hz label");
        Widgets::Label *sixtyHzLabel = new Widgets::Label(group);
		sixtyHzLabel->setText("60Hz");
		sixtyHzLabel->updateSize();
        Log::msg("Create 50 hz push button");
		_50hzFilter = new Widgets::PushButton(group);
		if(_manager.fiftyHzFilterEnabled())
        {
            Log::msg("50 hz enabled");
			_50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
        }
		else
        {
            Log::msg("50 hz disabled");
			_50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
        }
        Log::msg("Wire up 50 hz");
		_50hzFilter->setSizeHint(Widgets::Size(chckBoxSize,chckBoxSize));
		_50hzFilter->clicked.connect(this, &ConfigView::fiftyHzPressed);

        Log::msg("Create 60 hz filter button");
		_60hzFilter = new Widgets::PushButton(group);
		if(_manager.sixtyHzFilterEnabled())
        {
            Log::msg("60 Hz enabled");
			_60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
        }
		else
        {
            Log::msg("60 Hz disabled");
			_60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
        }
        Log::msg("Wire up 60 Hz");
		_60hzFilter->setSizeHint(Widgets::Size(chckBoxSize,chckBoxSize));
		_60hzFilter->clicked.connect(this, &ConfigView::sixtyHzPressed);

        Log::msg("Add components to filter box");
		filterhbox->addWidget(filterLabel, Widgets::AlignVCenter);
		filterhbox->addSpacing(13);
		Log::msg("50 Hz components");
		filterhbox->addWidget(fiftyHzLabel, Widgets::AlignVCenter);
		filterhbox->addSpacing(4);
		filterhbox->addWidget(_50hzFilter, Widgets::AlignVCenter);
		filterhbox->addSpacing(16);
		Log::msg("60 hz components");
		filterhbox->addWidget(sixtyHzLabel, Widgets::AlignVCenter);
		filterhbox->addSpacing(4);
		filterhbox->addWidget(_60hzFilter, Widgets::AlignVCenter);
		filterhbox->addSpacing(50);
		Log::msg("Add filter box to gvbox");
		gvbox->addLayout(filterhbox);
		gvbox->addSpacing(40);
	}


    //----------- Color chooser for channels --------------------------------

    joystickKeyDropdowns.resize(3);

    if(weAreOnTouchScreen)
    {
        Log::msg("Color chooser start touch");
        for(unsigned int i = 0; i < _manager.virtualDevices().size(); i++) {
            Log::msg("Create dropdown");
            _hclrs[i] = new HorizontalColorPicker(group);

            if(_manager.serialMode())
            {
                _hclrs[i]->setContent(arduinoColorSet);
            }
            else
            {
                _hclrs[i]->setContent(colorSet);
            }

            Log::msg("Signal catcher");
            _catchers.push_back(SignalCatcher(i, this));
            _hclrs[i]->selectionChanged.connect(&_catchers[i], &SignalCatcher::catchColor);
            Log::msg("Create label");
            Widgets::Label *name = new Widgets::Label(group);
            Log::msg("get name of virtual device");
            name->setText(_manager.virtualDevices()[i].name.c_str());
            name->updateSize();
            Log::msg("Create box layout for color chooser");
            Widgets::BoxLayout *ghbox = new Widgets::BoxLayout(Widgets::Horizontal);
            Log::msg("Add to gvbox");
            ghbox->addWidget(_hclrs[i]);
            ghbox->addSpacing(20);
            ghbox->addWidget(name,Widgets::AlignVCenter);
            gvbox->addLayout(ghbox);
            gvbox->addSpacing(15);
        }

    }
    else
    {
        Log::msg("Color chooser start -------");
        for(unsigned int i = 0; i < _manager.virtualDevices().size(); i++) {
            Log::msg("Create dropdown");


            _clrs[i] = new ColorDropDownList(group);

            if(_manager.serialMode())
            {
                _clrs[i]->setContent(arduinoColorSet);
            }
            else
            {
                _clrs[i]->setContent(colorSet);
            }

            Log::msg("Signal catcher");
            _catchers.push_back(SignalCatcher(i, this));
            _clrs[i]->selectionChanged.connect(&_catchers[_catchers.size()-1], &SignalCatcher::catchColor);


            //create dropdowns for selection of keys for Joystick
            if(_manager.currentAddOnBoard() == BOARD_WITH_JOYSTICK)
            {
                    Log::msg("Create dropdown for keys");
                    joystickKeyDropdowns[i] = new DropDownList(group, 100,30);
                    joystickKeyDropdowns[i]->clear();
                    Log::msg("Add items");
                    joystickKeyDropdowns[i]->addItem("None");
                    joystickKeyDropdowns[i]->addItem("w");
                    joystickKeyDropdowns[i]->addItem("s");
                    joystickKeyDropdowns[i]->addItem("a");
                    joystickKeyDropdowns[i]->addItem("d");
                    joystickKeyDropdowns[i]->addItem("z");
                    joystickKeyDropdowns[i]->addItem("q");
                    joystickKeyDropdowns[i]->addItem("c");
                    joystickKeyDropdowns[i]->addItem("v");
                    Log::msg("Set selection");

                    joystickKeyDropdowns[i]->setSelection(_manager.getKeyIndexForJoystick(i));

                    Log::msg("Signal catcher");
                    _catchers.push_back(SignalCatcher(i, this));
                    Log::msg("Connect other signal catchers");
                    joystickKeyDropdowns[i]->indexChanged.connect(&_catchers[_catchers.size()-1], &SignalCatcher::changeKeyForJoystick);
            }

            Log::msg("Create label");
            Widgets::Label *name = new Widgets::Label(group);
            Log::msg("get name of virtual device");
            name->setText(_manager.virtualDevices()[i].name.c_str());
            name->updateSize();
            Log::msg("Create box layout for color chooser");
            Widgets::BoxLayout *ghbox = new Widgets::BoxLayout(Widgets::Horizontal);
            Log::msg("Add to gvbox");
            ghbox->addWidget(_clrs[i]);
#if defined(_WIN32)
            if(_manager.currentAddOnBoard() == BOARD_WITH_JOYSTICK)
            {
                ghbox->addSpacing(20);
                ghbox->addWidget(joystickKeyDropdowns[i]);
            }
#endif
            ghbox->addSpacing(20);
            ghbox->addWidget(name,Widgets::AlignVCenter);
            gvbox->addLayout(ghbox);
            gvbox->addSpacing(15);
        }
    }


    Log::msg("Set selection");
	for(int i = 0; i < _audioView.channelCount(); i++)
    {
        Log::msg("Set selection n");
        if(weAreOnTouchScreen)
        {
            _hclrs[_audioView.channelVirtualDevice(i)]->setSelection(_audioView.channelColor(i));
        }
        else
        {
            _clrs[_audioView.channelVirtualDevice(i)]->setSelection(_audioView.channelColor(i));
        }
    }




    // USB HID and serial devices share one input chooser. A HID device does
    // not have a COM port and must never be presented as a serial failure.
    serialPortWidget=NULL;touchSerialPortWidget=NULL;_connectButton=NULL;_usbStatus=NULL;
    if(!_manager.fileMode())
    {
        Widgets::Label *name2=new Widgets::Label(group);
        name2->setText("USB sensor / device");name2->updateSize();
        gvbox->addWidget(name2,Widgets::AlignLeft);
        Widgets::BoxLayout *serialHbox=new Widgets::BoxLayout(Widgets::Horizontal);
        if(weAreOnTouchScreen) {
            touchSerialPortWidget=new TouchDropDownList(group,360,40);
            touchSerialPortWidget->clear();
            touchSerialPortWidget->indexChanged.connect(this,&ConfigView::usbSelectionChanged);
            serialHbox->addWidget(touchSerialPortWidget,Widgets::AlignVCenter);
        } else {
            serialPortWidget=new DropDownList(group,360,30);
            serialPortWidget->clear();
            serialPortWidget->indexChanged.connect(this,&ConfigView::usbSelectionChanged);
            serialHbox->addWidget(serialPortWidget,Widgets::AlignVCenter);
        }
        _connectButton=new USBConnectButton(group);
        _connectButton->setSizeHint(Widgets::Size(110,weAreOnTouchScreen?40:32));
        _connectButton->clicked.connect(this,&ConfigView::connectPressed);
        serialHbox->addSpacing(8);serialHbox->addWidget(_connectButton,Widgets::AlignVCenter);
        gvbox->addLayout(serialHbox);
        _usbStatus=new Widgets::Label(group);gvbox->addWidget(_usbStatus,Widgets::AlignLeft);
        _manager.scanForHIDDevices();
        _usbInputs.clear();refreshUSBInputs();

        //-------------- Serial Number of channels chooser ----------------------------------------
        Log::msg("Check if in serial mode for Num. ch. dropdown");
        if(_manager.serialMode() &&
           !(_manager.getCurrentPort().deviceType == ArduinoSerial::heartOneChannel
             || _manager.getCurrentPort().deviceType == ArduinoSerial::humansb
             || _manager.getCurrentPort().deviceType == ArduinoSerial::hhibox
             || _manager.getCurrentPort().deviceType == ArduinoSerial::plant
             || _manager.getCurrentPort().deviceType == ArduinoSerial::muscleusb
             || _manager.getCurrentPort().deviceType == ArduinoSerial::extclaw
             || _manager.getCurrentPort().deviceType == ArduinoSerial::sbpromusclecdc
             || _manager.getCurrentPort().deviceType == ArduinoSerial::sbproneuroncdc
             || _manager.getCurrentPort().deviceType == ArduinoSerial::sbproneuronmfi
             || _manager.getCurrentPort().deviceType == ArduinoSerial::unibox 
             || _manager.getCurrentPort().deviceType == ArduinoSerial::heartPro
             || _manager.getCurrentPort().deviceType == ArduinoSerial::neuronOneChannel))
        {
                //Number of channels chooser
                Log::msg("Create box layout");
                Widgets::BoxLayout *numberOfChannelsHbox = new Widgets::BoxLayout(Widgets::Horizontal);

                Log::msg("Create label");
                Widgets::Label *numChannelsLabel = new Widgets::Label(group);
                if(weAreOnTouchScreen)
                {
                    numChannelsLabel->setText("Num. of channels:");
                }
                else
                {
                    numChannelsLabel->setText("Number of channels:");
                }

                numChannelsLabel->updateSize();
                Log::msg("Add label to box");
                numberOfChannelsHbox->addWidget(numChannelsLabel, Widgets::AlignVCenter);
                numberOfChannelsHbox->addSpacing(5);

                if(weAreOnTouchScreen)
                {

                    HorizontalNumberPicker * horizontalNumberPicker = new HorizontalNumberPicker(group);
                    horizontalNumberPicker->setLimits(1, 6);
                    horizontalNumberPicker->setSelection(_manager.numberOfSerialChannels());

                    _catchers.push_back(SignalCatcher(_catchers.size(), this));

                    horizontalNumberPicker->selectionChanged.connect(&_catchers[_catchers.size()-1], &SignalCatcher::setNumOfChannelsForTouchHandler);
                    //horizontalNumberPicker->setDisabled(!_manager.serialMode());
                    numberOfChannelsHbox->addWidget(horizontalNumberPicker);
                }
                else
                {
                    Log::msg("Create dropdown");
                    numberOfChannelsWidget = new DropDownList(group, 50,30);
                    numberOfChannelsWidget->clear();
                    Log::msg("Add items");
                    numberOfChannelsWidget->addItem("1");
                    numberOfChannelsWidget->addItem("2");
                    numberOfChannelsWidget->addItem("3");
                    numberOfChannelsWidget->addItem("4");
                    numberOfChannelsWidget->addItem("5");
                    numberOfChannelsWidget->addItem("6");
                    Log::msg("Set selection");
                    numberOfChannelsWidget->setSelection(_manager.numberOfSerialChannels()-1);
                    Log::msg("Signal catcher");
                    _catchers.push_back(SignalCatcher(_catchers.size(), this));
                    Log::msg("Connect other signal catchers");
                    numberOfChannelsWidget->indexChanged.connect(&_catchers[_catchers.size()-1], &SignalCatcher::setNumOfChannelsHandler);
                    Log::msg("Disable/enable");
                    numberOfChannelsWidget->setDisabled(!_manager.serialMode());
                    Log::msg("Add widget to num of channel HBox");
                    numberOfChannelsHbox->addWidget(numberOfChannelsWidget, Widgets::AlignVCenter);
                }

                numberOfChannelsHbox->update();
                Log::msg("Add layout num of ch box");
                gvbox->addSpacing(10);
                gvbox->addLayout(numberOfChannelsHbox);
        }
        
        //firmware update for STM32
        if(_manager.firmwareUpdateShouldBeActive())
        {
             Widgets::BoxLayout *updateBootloaderHbox = new Widgets::BoxLayout(Widgets::Horizontal);
            //Add label
            Widgets::Label *updateBootloaderLabel = new Widgets::Label(group);

            Widgets::BoxLayout *updateBootloaderVbox = new Widgets::BoxLayout(Widgets::Vertical);

            if(_manager.getCurrentPort().deviceType == ArduinoSerial::unibox)
            {
                updateBootloaderLabel->setText("Update firmware for Spike Station?");
            }
            else
            {
                updateBootloaderLabel->setText("Update firmware for SpikerBox?");
            }
            
            updateBootloaderLabel->updateSize();
            updateBootloaderHbox->addSpacing(0);
            updateBootloaderVbox->addSpacing(7);
            updateBootloaderVbox->addWidget(updateBootloaderLabel);
            updateBootloaderHbox->addLayout(updateBootloaderVbox);
            updateBootloaderHbox->addSpacing(156);
            
            _updateBootloaderButton = new Widgets::PushButton(group);
            
            _updateBootloaderButton->clicked.connect(this, &ConfigView::firmwareBootloaderUpdatePressed);

            if(weAreOnTouchScreen)
            {
                _updateBootloaderButton->setNormalTex(Widgets::TextureGL::get("data/bupdate.bmp"));
                _updateBootloaderButton->setHoverTex(Widgets::TextureGL::get("data/bupdate-high.bmp"));
                _updateBootloaderButton->setSizeHint(Widgets::Size(100,40));
            }
            else
            {
                _updateBootloaderButton->setNormalTex(Widgets::TextureGL::get("data/update.bmp"));
                _updateBootloaderButton->setHoverTex(Widgets::TextureGL::get("data/update-high.bmp"));
                _updateBootloaderButton->setSizeHint(Widgets::Size(100,32));
            }

            updateBootloaderHbox->addSpacing(5);
            updateBootloaderHbox->addWidget(_updateBootloaderButton, Widgets::AlignVCenter);
            updateBootloaderHbox->update();

            gvbox->addSpacing(25);
            gvbox->addLayout(updateBootloaderHbox);
  
        }

        // -------------------- HID device connect (works only under windows)---------------------------------------

        #if defined(_WIN32)

        //--------------   Update firmware code (works only under Windows)----------------------


        Log::msg("Check HID mode");
        if(_manager.hidMode())
        {
                    Log::msg("Crete box layout");
                     Widgets::BoxLayout *updateHbox = new Widgets::BoxLayout(Widgets::Horizontal);
                    //Add label
                    Widgets::Label *updateLabel = new Widgets::Label(group);
                    Log::msg("Create label");
                    std::stringstream labelTextString;
                    if(_manager.firmwareAvailable())
                    {
                        Log::msg("Firmware available");
                        labelTextString <<"Change firmware (device "<< _manager.currentHIDFirmwareType()<<" F:"<<_manager.currentHIDFirmwareVersion()<<" H:"<<_manager.currentHIDHardwareVersion()<<"):";
                    }
                    else
                    {
                        Log::msg("Connected device");
                        labelTextString <<"Connected device: "<< _manager.currentHIDFirmwareType()<<" F:"<<_manager.currentHIDFirmwareVersion()<<" H:"<<_manager.currentHIDHardwareVersion()<<"";
                    }
                    Log::msg("Set text");
                    updateLabel->setText(labelTextString.str().c_str());
                    updateLabel->updateSize();
                    updateHbox->addSpacing(0);
                    Log::msg("Add widget");
                    updateHbox->addWidget(updateLabel, Widgets::AlignLeft);
                    updateHbox->addSpacing(5);

                    Log::msg("Create update dropdown");
                    Widgets::BoxLayout *updateSelectionHbox = new Widgets::BoxLayout(Widgets::Horizontal);
                    if(_manager.firmwareAvailable())
                    {
                        //Add dropbox
                        Log::msg("Firmware available add dropbox");

                        if(weAreOnTouchScreen)
                        {
                                    touchFirmwareListWidget = new TouchDropDownList(group);
                                    touchFirmwareListWidget->clear();
                                    std::list<BYBFirmwareVO> fps =  _manager.firmwareList();
                                    for( listBYBFirmwareVO::iterator ti = fps.begin();
                                        ti != fps.end();
                                        ti ++)
                                    {
                                        std::stringstream sstm;
                                        if(((BYBFirmwareVO)(*ti)).location == REMOTE_FIRMWARE)
                                        {
                                            sstm <<((BYBFirmwareVO)(*ti)).description << " (V" << ((BYBFirmwareVO)(*ti)).version <<")";
                                        }
                                        else
                                        {
                                            sstm <<((BYBFirmwareVO)(*ti)).description;
                                        }
                                        touchFirmwareListWidget->addItem(sstm.str().c_str());
                                    }

                                    touchFirmwareListWidget->setSelection(0);
                                    selectedFirmware = new BYBFirmwareVO();
                                    selectedFirmware->URL = std::string((_manager.firmwareList().front()).URL);
                                    selectedFirmware->filepath = std::string((_manager.firmwareList().front()).filepath);
                                    selectedFirmware->id = (_manager.firmwareList().front()).id;
                                    touchFirmwareListWidget->setDisabled(false);
                                    _catchers.push_back(SignalCatcher(_catchers.size(), this));
                                    touchFirmwareListWidget->indexChanged.connect(&_catchers[_catchers.size()-1], &SignalCatcher::catchFirmwareSelection);
                                    updateSelectionHbox->addWidget(touchFirmwareListWidget,Widgets::AlignVCenter);

                        }
                        else
                        {
                                    firmwaresWidget = new DropDownList(group,400,30);
                                    firmwaresWidget->clear();
                                    std::list<BYBFirmwareVO> fps =  _manager.firmwareList();
                                    for( listBYBFirmwareVO::iterator ti = fps.begin();
                                        ti != fps.end();
                                        ti ++)
                                    {
                                        std::stringstream sstm;
                                        if(((BYBFirmwareVO)(*ti)).location == REMOTE_FIRMWARE)
                                        {
                                            sstm <<((BYBFirmwareVO)(*ti)).description << " (V" << ((BYBFirmwareVO)(*ti)).version <<")";
                                        }
                                        else
                                        {
                                            sstm <<((BYBFirmwareVO)(*ti)).description;
                                        }
                                        firmwaresWidget->addItem(sstm.str().c_str() );
                                    }

                                    firmwaresWidget->setSelection(0);
                                    selectedFirmware = new BYBFirmwareVO();
                                    selectedFirmware->URL = std::string((_manager.firmwareList().front()).URL);
                                    selectedFirmware->filepath = std::string((_manager.firmwareList().front()).filepath);
                                    selectedFirmware->id = (_manager.firmwareList().front()).id;
                                    firmwaresWidget->setDisabled(false);
                                    _catchers.push_back(SignalCatcher(_catchers.size(), this));
                                    firmwaresWidget->indexChanged.connect(&_catchers[_catchers.size()-1], &SignalCatcher::catchFirmwareSelection);
                                    updateSelectionHbox->addWidget(firmwaresWidget, Widgets::AlignVCenter);
                        }

                        Log::msg("Create button for update");
                        //Button for update HID device
                        _updateButton = new Widgets::PushButton(group);

                        _updateButton->clicked.connect(this, &ConfigView::firmwareUpdatePressed);
                        Log::msg("Set image for update button");
                        if(weAreOnTouchScreen)
                        {
                             _updateButton->setNormalTex(Widgets::TextureGL::get("data/bupdate.bmp"));
                            _updateButton->setHoverTex(Widgets::TextureGL::get("data/bupdate-high.bmp"));
                            _updateButton->setSizeHint(Widgets::Size(100,40));
                        }
                        else
                        {
                           _updateButton->setNormalTex(Widgets::TextureGL::get("data/update.bmp"));
                            _updateButton->setHoverTex(Widgets::TextureGL::get("data/update-high.bmp"));
                            _updateButton->setSizeHint(Widgets::Size(100,32));
                        }


                        Log::msg("Add button widget");
                        updateSelectionHbox->addSpacing(5);
                        updateSelectionHbox->addWidget(_updateButton, Widgets::AlignVCenter);
                        updateSelectionHbox->update();
                    }

                    Log::msg("Add spacing");
                    gvbox->addSpacing(20);
                    gvbox->addLayout(updateHbox);
                    Log::msg("Firmware available?");
                    if(_manager.firmwareAvailable())
                    {
                        Log::msg("Add layout");
                        gvbox->addLayout(updateSelectionHbox);
                    }

            Log::msg("End");
        }
        #endif
        Log::msg("End in file mode");
    }//end if not file mode

    Log::msg("Update layout for gv box");
	gvbox->update();

    Log::msg("Create vbox and hbox");
	vbox = new Widgets::BoxLayout(Widgets::Vertical, this);
	Widgets::BoxLayout *hbox = new Widgets::BoxLayout(Widgets::Horizontal);
	hbox->addSpacing(10);
	Log::msg("Add close button");
	hbox->addWidget(closeButton);
	hbox->addSpacing(17);
	Log::msg("Add top label");
	hbox->addWidget(topLabel, Widgets::AlignVCenter);
	vbox->addSpacing(10);
	Log::msg("Add hbox");
	vbox->addLayout(hbox);
	vbox->addSpacing(20);
	Log::msg("Add group");
	vbox->addWidget(group, Widgets::AlignCenter);

	vbox->update();
    this->setSize(this->size());

    Log::msg("End function");
}
//----------------------------------- END OF CONFIG ------------------------------------------------
//--------------------------------------------------------------------------------------------------

void ConfigView::paintEvent() {
    const uint32_t now=SDL_GetTicks();
    if(now-_lastUSBRefresh>=500) {_lastUSBRefresh=now;refreshUSBInputs();}


    if(weAreOnTouchScreenOld != Widgets::Application::getInstance()->areWeOnTouchscreen())
    {
        changeScreenType = true;
        weAreOnTouchScreenOld = Widgets::Application::getInstance()->areWeOnTouchscreen();
    }
    
    //change filter values if they are changed externaly
    if(_manager.checkIfFreqChangedExternaly())
    {
        _manager.resetFlagForFreqChangedExternaly();
        
        highFilterValueChanged(_manager.lowCornerFrequency());
        lowFilterValueChanged(_manager.highCornerFrequency());
        
        if(rangeSelector->getLowValue()>_manager.lowCornerFrequency())
        {
            rangeSelector->setLowValue(_manager.highCornerFrequency());
            rangeSelector->setHighValue(_manager.lowCornerFrequency());
        }
        else
        {
            rangeSelector->setHighValue(_manager.lowCornerFrequency());
            rangeSelector->setLowValue(_manager.highCornerFrequency());
        }
        if(_manager.fiftyHzFilterEnabled())
        {
            _60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
            _50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
        }
        else if(_manager.sixtyHzFilterEnabled())
        {
            _60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
            _50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
        }
        else if(!_manager.sixtyHzFilterEnabled() && !_manager.fiftyHzFilterEnabled())
        {
            _60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
            _50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
        }
    }
    if(changeScreenType)
    {
        cleanWholeScreenAndResetup();
    }
	Widgets::Color bg = Widgets::Colors::background;
	bg.a = 255;
	Widgets::Painter::setColor(bg);
	Widgets::Painter::drawRect(rect());

}

void ConfigView::firmwareBootloaderUpdatePressed()
{
    _manager.putBoardInBootloaderMode();
    close();
}

void ConfigView:: presetPlantPressed()
{
    _manager.setPresetFilters(0, PRESET_MESSAGE_VALUE_PLANT);
    _manager.enable60HzFilter();
    _60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
    _50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
}
void ConfigView:: presetNeuronPressed()
{
    _manager.setPresetFilters(0, PRESET_MESSAGE_VALUE_EXTNEUR);
    _manager.enable60HzFilter();
    _60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
    _50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
}
void ConfigView:: presetEEGPressed()
{
    _manager.setPresetFilters(0, PRESET_MESSAGE_VALUE_EEG);
    _manager.enable60HzFilter();
    _60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
    _50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
}
void ConfigView:: presetECGPressed()
{
    _manager.setPresetFilters(0, PRESET_MESSAGE_VALUE_ECG);
    _manager.enable60HzFilter();
    _60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
    _50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
}
void ConfigView:: presetEMGPressed()
{
    _manager.setPresetFilters(0, PRESET_MESSAGE_VALUE_EMG);
    _manager.enable60HzFilter();
    _60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
    _50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
}

void ConfigView::checkIfWeShouldHighlightPreset()
{
    int hpf = _manager.highCornerFrequency();
    int lpf = _manager.lowCornerFrequency();
    if(hpf==0 && lpf==50)
    {
        _presetEEGButton->setNormalTex(Widgets::TextureGL::get("data/preset-eeg-high.bmp"));
        _presetEEGButton->setHoverTex(Widgets::TextureGL::get("data/preset-eeg-high.bmp"));
    }
    else
    {
        _presetEEGButton->setNormalTex(Widgets::TextureGL::get("data/preset-eeg.bmp"));
        _presetEEGButton->setHoverTex(Widgets::TextureGL::get("data/preset-eeg-high.bmp"));
    }
    
    if(hpf==70 && lpf==2500)
    {
        _presetEMGButton->setNormalTex(Widgets::TextureGL::get("data/preset-emg-high.bmp"));
        _presetEMGButton->setHoverTex(Widgets::TextureGL::get("data/preset-emg-high.bmp"));
    }
    else
    {
        _presetEMGButton->setNormalTex(Widgets::TextureGL::get("data/preset-emg.bmp"));
        _presetEMGButton->setHoverTex(Widgets::TextureGL::get("data/preset-emg-high.bmp"));
    }
    
    if(hpf==0 && lpf==5)
    {
        _presetPlantButton->setNormalTex(Widgets::TextureGL::get("data/preset-plant-high.bmp"));
        _presetPlantButton->setHoverTex(Widgets::TextureGL::get("data/preset-plant-high.bmp"));
    }
    else
    {
        _presetPlantButton->setNormalTex(Widgets::TextureGL::get("data/preset-plant.bmp"));
        _presetPlantButton->setHoverTex(Widgets::TextureGL::get("data/preset-plant-high.bmp"));
    }
    
    if(hpf==1 && lpf==100)
    {
        _presetECGButton->setNormalTex(Widgets::TextureGL::get("data/preset-ecg-high.bmp"));
        _presetECGButton->setHoverTex(Widgets::TextureGL::get("data/preset-ecg-high.bmp"));
    }
    else
    {
        _presetECGButton->setNormalTex(Widgets::TextureGL::get("data/preset-ecg.bmp"));
        _presetECGButton->setHoverTex(Widgets::TextureGL::get("data/preset-ecg-high.bmp"));
    }
    
    if(hpf==70 && lpf==_manager.sampleRate()/2)
    {
        _presetNeuronButton->setNormalTex(Widgets::TextureGL::get("data/preset-neuron-high.bmp"));
        _presetNeuronButton->setHoverTex(Widgets::TextureGL::get("data/preset-neuron-high.bmp"));
    }
    else
    {
        _presetNeuronButton->setNormalTex(Widgets::TextureGL::get("data/preset-neuron.bmp"));
        _presetNeuronButton->setHoverTex(Widgets::TextureGL::get("data/preset-neuron-high.bmp"));
    }
}

#if defined(_WIN32)

      /*  void ConfigView::hidConnectPressed()
        {
            Log::msg("Hid connect pressed");
            //connect/diconnect

            if(_manager.hidMode())
            {
                // _manager.setSerialNumberOfChannels(1);
                _manager.disconnectFromHID();

                _hidButton->setNormalTex(Widgets::TextureGL::get("data/connected.bmp"));
                _hidButton->setHoverTex(Widgets::TextureGL::get("data/connected-high.bmp"));
                close();
            }
            else
            {
                if(!_manager.initHIDUSB())
                {
                    std::cout<<"Can't open HID device. \n";


                    Widgets::ErrorBox *box = new Widgets::ErrorBox(_manager.hidError.c_str());
                    box->setGeometry(Widgets::Rect(this->width()/2-250, this->height()/2-40, 500, 80));
                    Widgets::Application::getInstance()->addPopup(box);
                }
                _hidButton->setNormalTex(Widgets::TextureGL::get("data/disconnected.bmp"));
                _hidButton->setHoverTex(Widgets::TextureGL::get("data/disconnected-high.bmp"));
                close();
            }
        }
*/
        //
        //Firmware selection drop-down selection changed
        //
        void ConfigView::firmwareSelectionChanged(int firmwareid)
        {
            Log::msg("Firmware selection changed");
             std::list<BYBFirmwareVO> fps =  _manager.firmwareList();

            int i = 0;
            for( listBYBFirmwareVO::iterator ti = fps.begin();
                ti != fps.end();
                ti ++)
            {
                if(i==firmwareid)
                {
                    //selectedFirmware = (BYBFirmwareVO*)&(*ti);
                    selectedFirmware = new BYBFirmwareVO();
                    selectedFirmware->URL = std::string((*ti).URL);
                    selectedFirmware->filepath = std::string((*ti).filepath);
                    selectedFirmware->id = (*ti).id;
                    selectedFirmware->location = (*ti).location;
                    break;
                }
                i++;
            }
        }



        //
        // Start firmware update procedure
        //
        void ConfigView::firmwareUpdatePressed()
        {
            Log::msg("Firmware update pressed");
            if(_manager.getUSBFirmwareUpdateStage() == 0)//avoid starting it twice
            {
                //send message to MSP to prepare for update
                std::cout<<"\n\n\n"<<"Firmware URL: "<<selectedFirmware->URL<<"\n\n\n";
                if(_manager.prepareForHIDFirmwareUpdate(selectedFirmware))
                {
                    std::cout<<"Error downloading firmware";

                    const char *error = "Error while downloading firmware.\n  Check your Internet connection.";
                    Widgets::ErrorBox *box = new Widgets::ErrorBox(error);
                    box->setGeometry(Widgets::Rect(this->width()/2-150, this->height()/2-40, 300, 80));
                    Widgets::Application::getInstance()->addPopup(box);

                }
                close();
            }
        }
#endif

//
// Connect/dsconnect from serial port
//
void ConfigView::connectPressed()
{
    Log::msg("USB connect pressed");
    // Preserve the selected device identity, even if scanning reorders the list.
    const USBInputChoice selected=_selectedUSB;
    const bool hadSelection=_hasSelectedUSB;
    refreshUSBInputs();
    std::string error;
    const int index=hadSelection?usbInputIndex(_usbInputs,selected):-1;
    if(index<0) {
        error="USB sensor not detected. Check the sensor power and USB data cable, then reconnect. HID devices do not require a serial COM port.";
    } else {
        const USBInputChoice choice=_usbInputs[static_cast<size_t>(index)];
        const bool connectedHID=choice.hid!=HID_BOARD_TYPE_NONE && _manager.hidMode() && _manager.currentlyConnectedHIDBoardType()==choice.hid;
        const bool connectedSerial=choice.hid==HID_BOARD_TYPE_NONE && _manager.serialMode() && ArduinoSerial::samePortName(_manager.getCurrentPort().portName,choice.serial.portName);
        if(connectedHID || connectedSerial) {
            if(connectedHID)_manager.disconnectFromHID();
            else _manager.disconnectFromSerial();
            close();return;
        }
        const bool connected=choice.hid!=HID_BOARD_TYPE_NONE?
            _manager.initHIDUSB(choice.hid):_manager.initSerial(choice.serial.portName.c_str(),choice.serial.baudRate);
        if(connected) {
            if(_manager.paused())_manager.setPaused(false);
            _audioView.resetLiveDisplay();
            close();return;
        }
        error=choice.hid!=HID_BOARD_TYPE_NONE?_manager.hidError:_manager.serialError;
        if(error.empty())error="The USB device could not be opened. Close other programs using the sensor and try again.";
    }
    // Keep settings open on failure so the user can retry or choose a device.
    Log::error("%s",error.c_str());
    Widgets::ErrorBox *box=new Widgets::ErrorBox(error.c_str());
    box->setGeometry(Widgets::Rect(width()/2-250,height()/2-60,500,120));
    Widgets::Application::getInstance()->addPopup(box);
}

void ConfigView::usbSelectionChanged(int index)
{
    if(_refreshingUSB)return;
    _hasSelectedUSB=index>=0 && index<static_cast<int>(_usbInputs.size());
    if(_hasSelectedUSB)_selectedUSB=_usbInputs[static_cast<size_t>(index)];
}

void ConfigView::refreshUSBInputs()
{
    if(!_usbStatus || !_connectButton)return;
    const int activeHID=_manager.hidMode()?_manager.currentlyConnectedHIDBoardType():HID_BOARD_TYPE_NONE;
    auto choices=usbInputChoices(_manager.serailPorts(),_manager.isHIDBoardTypeAvailable(HID_BOARD_TYPE_MUSCLE)||activeHID==HID_BOARD_TYPE_MUSCLE,
        _manager.isHIDBoardTypeAvailable(HID_BOARD_TYPE_NEURON)||activeHID==HID_BOARD_TYPE_NEURON);
    USBInputChoice active;active.hid=static_cast<HIDBoardType>(activeHID);active.serial=_manager.getCurrentPort();
    if(_manager.serialMode() && usbInputIndex(choices,active)<0) {
        active.label=std::string(serialDeviceTitle(active.serial.deviceType))+" ("+active.serial.portName+")";choices.push_back(active);
    }
    int selection=_hasSelectedUSB?usbInputIndex(choices,_selectedUSB):-1;
    if(selection<0 && !_hasSelectedUSB && (_manager.serialMode() || _manager.hidMode()))selection=usbInputIndex(choices,active);
    if(selection<0 && !choices.empty())selection=0;
    bool changed=choices.size()!=_usbInputs.size();
    if(!changed)for(size_t i=0;i<choices.size();++i)if(!choices[i].sameOption(_usbInputs[i]))changed=true;
    if(changed) {
        _refreshingUSB=true;_usbInputs=choices;
        if(weAreOnTouchScreen) {
            touchSerialPortWidget->clear();for(const auto &c:choices)touchSerialPortWidget->addItem(c.label);
            touchSerialPortWidget->setSelection(std::max(0,selection));touchSerialPortWidget->setDisabled(choices.empty());
        } else {
            serialPortWidget->clear();for(const auto &c:choices)serialPortWidget->addItem(c.label);
            serialPortWidget->setSelection(std::max(0,selection));serialPortWidget->setDisabled(choices.empty());
        }
        _refreshingUSB=false;
    }
    if(weAreOnTouchScreen)touchSerialPortWidget->setDisabled(choices.empty());
    else serialPortWidget->setDisabled(choices.empty());
    _hasSelectedUSB=selection>=0;
    if(_hasSelectedUSB)_selectedUSB=choices[static_cast<size_t>(selection)];
    const bool selectedConnected=_hasSelectedUSB && (_manager.serialMode() || _manager.hidMode()) && _selectedUSB.sameDevice(active);
    static_cast<USBConnectButton*>(_connectButton)->setConnected(selectedConnected);
    const char *status=_manager.hidMode()?"USB HID connected - no COM port required.":_manager.serialMode()?"USB serial connected.":choices.empty()?"No USB sensor detected. Check power and USB data cable.":"USB device detected. Select it and press Connect.";
    _usbStatus->setText(status);_usbStatus->updateSize();
}

void ConfigView::closePressed() {
    Log::msg("Close pressed");
	close();
}

void ConfigView::mutePressed() {
    Log::msg("Mute pressed");
	if(_manager.player().volume() == 0) {
		_muteCKBox->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
		_manager.player().setVolume(100);
	} else {
		_muteCKBox->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
		_manager.player().setVolume(0);
	}
}

//
// Band pass filtering value changed
//
void ConfigView::highFilterValueChanged(int hvalue)
{
    Log::msg("High filter value changed");
    highValueTI->setInt(hvalue);
    if(hvalue>=_manager.sampleRate()/2)
    {
        _manager.enableLowPassFilterWithCornerFreq(_manager.sampleRate()/2);
        _manager.disableLowPassFilter();
    }
    else
    {
        _manager.enableLowPassFilterWithCornerFreq(hvalue);
    }
    _manager.setLPFOnSerial(0, hvalue);
    checkIfWeShouldHighlightPreset();
}

#define THRESHOLD_FOR_HPF 20
//
// Band pass filtering value changed
//
void ConfigView::lowFilterValueChanged(int lvalue)
{
    Log::msg("Low filter value changed");

    lowValueTI->setInt(lvalue);
    
     if(lvalue<=0)
    {
        Log::msg("First if");
        _manager.enableHighPassFilterWithCornerFreq(0.0);
         _manager.disableHighPassFilter();
    }
    else
    {
        Log::msg("else");
        _manager.enableHighPassFilterWithCornerFreq(lvalue);
        // _manager.setHPFOnSerial(0, 22.467);
    }
    checkIfWeShouldHighlightPreset();
}

void ConfigView::lowFilterTIValueChanged(std::string newString)
{
    Log::msg("lowFilterTIValueChanged");
    rangeSelector->setLowValue(lowValueTI->getInt());
    lowValueTI->setInt(rangeSelector->getLowValue());
    checkIfWeShouldHighlightPreset();
}

void ConfigView::highFilterTIValueChanged(std::string newString)
{
    Log::msg("highFilterTIValueChanged");
    rangeSelector->setHighValue(highValueTI->getInt());
    highValueTI->setInt(rangeSelector->getHighValue());
    checkIfWeShouldHighlightPreset();
}

void ConfigView::cleanWholeScreenAndResetup()
{
    changeScreenType = false;
    weAreOnTouchScreen = !weAreOnTouchScreen;
    gvbox->removeAll();
    vbox->removeAll();
    SetupScreen();
    
    return;
}

//
// Enable/disable 50Hz filter checkbox pressed
//
void ConfigView::fiftyHzPressed() {


    //changeScreenType = true;
    Log::msg("fiftyHzPressed");
    if(_manager.fiftyHzFilterEnabled())
    {
        Log::msg("Enabled");
        _manager.disable50HzFilter();
        _60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
        _50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
    }
    else
    {
        Log::msg("Disabled");
        _manager.enable50HzFilter();
        _50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
        _60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
    }
}


//
// Enable/disable 60Hz filter checkbox pressed
//
void ConfigView::sixtyHzPressed() {
    Log::msg("60 hz pressed");
    if(_manager.sixtyHzFilterEnabled())
    {
        Log::msg("if");
        _manager.disable60HzFilter();
        _60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
        _50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
    }
    else
    {
        Log::msg("else");
        _manager.enable60HzFilter();
        _60hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxoff.bmp"));
        _50hzFilter->setNormalTex(Widgets::TextureGL::get("data/ckboxon.bmp"));
    }
}


void ConfigView::serialPortChanged(int virtualDevice, int portidx)
{
    Log::msg("Serial port changed");
    usbSelectionChanged(portidx);
}

void ConfigView::setSerialNumberOfChannels(int numberOfChannels)
{
    Log::msg("Set serial num of channels pressed");
    _manager.setSerialNumberOfChannels(numberOfChannels);
    close();
}

void ConfigView::colorChanged(int virtualDevice, int coloridx) {
    Log::msg("Color changed");
	int channel = _audioView.virtualDeviceChannel(virtualDevice);
	if(channel < 0 && coloridx != 0) {
            Log::msg("Have channels");
		bool success = _manager.bindVirtualDevice(virtualDevice);

		if(success) {
		    Log::msg("success");
			_audioView.setChannelColor(_audioView.channelCount()-1, coloridx);
		} else {
		    Log::msg("else");
            if(weAreOnTouchScreen)
            {
                _clrs[virtualDevice]->setSelectionSilent(0);
            }
            else
            {
                _hclrs[virtualDevice]->setSelectionSilent(0);
            }

			std::stringstream s;
			s << "Error: Cannot open channel";
			if(BASS_ErrorGetCode())
				s << ": " << GetBassStrError();
			Widgets::ErrorBox *box = new Widgets::ErrorBox(s.str().c_str());
			box->setGeometry(Widgets::Rect(this->width()/2-200, this->height()/2-40, 400, 80));
			Widgets::Application::getInstance()->addPopup(box);
		}
	} else if(coloridx == 0) {
	    Log::msg("==0");
		_manager.unbindVirtualDevice(virtualDevice);
	} else {
	    Log::msg("else else");
		_audioView.setChannelColor(channel, coloridx);
	}
}


void ConfigView::calibratePressed()
{
    Log::msg("Calibration pressed");
    close();
    CalibrationWindow * c = new CalibrationWindow(_manager,_audioView);
    c->setMouseTracking(true);
	c->setDeleteOnClose(true);
	c->setGeometry(Widgets::Rect(240, 20, 380, 160));
	Widgets::Application::getInstance()->addWindow(c);

}


void ConfigView::changeKeysForJoystick(int channelIndex, int keyIndex)
{
    _manager.setKeyForJoystick(channelIndex, keyIndex);
}

}
