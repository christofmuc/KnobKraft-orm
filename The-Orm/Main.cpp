/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "JuceHeader.h"

#include "MainComponent.h"

#include "Settings.h"
#include "UIModel.h"

#include "GenericAdaption.h"
#include "embedded_module.h"

#include <memory>

#include "version.cpp"


//==============================================================================
class TheOrmApplication  : public JUCEApplication
{
public:
    //==============================================================================
	TheOrmApplication() = default;

    const String getApplicationName() override       { return ProjectInfo::projectName; }
    const String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override       { return true; }

    //==============================================================================
    void initialise (const String& commandLine) override
    {
		ignoreUnused(commandLine);

        // This method is where you should put your application's initialization code..
		Settings::setSettingsID("KnobKraftOrm");

		// Init python for GenericAdaption
		knobkraft::GenericAdaption::startupGenericAdaption();

		// Init python with the embedded pytschirp module
		globalImportEmbeddedModules();

		// Select colour scheme
		auto lookAndFeel = &LookAndFeel_V4::getDefaultLookAndFeel();
		auto v4 = dynamic_cast<LookAndFeel_V4 *>(lookAndFeel);
		if (v4) {
			v4->setColourScheme(LookAndFeel_V4::getMidnightColourScheme());
		}
		mainWindow = std::make_unique<MainWindow> (getApplicationName() + String(" - Sysex Librarian V" + getOrmVersion())); 
    }

    void shutdown() override
    {
        // Add your application's shutdown code here..

        mainWindow = nullptr; // (deletes our window)

		// The UI is gone, we don't need the UIModel anymore
		UIModel::shutdown();

		// Shutdown MIDI subsystem after all windows are gone
		midikraft::MidiController::shutdown();

		// Shutdown settings subsystem
		Settings::instance().saveAndClose();
		Settings::shutdown();
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted (const String& commandLine) override
    {
		ignoreUnused(commandLine);
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
    }

    //==============================================================================
    /*
        This class implements the desktop window that contains an instance of
        our MainComponent class.
    */
    class MainWindow    : public DocumentWindow
    {
    public:
        MainWindow (String name)  : DocumentWindow (name,
                                                    Desktop::getInstance().getDefaultLookAndFeel()
                                                                          .findColour (ResizableWindow::backgroundColourId),
                                                    DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
           #endif

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            // This is called when the user tries to close this window. Here, we'll just
            // ask the app to quit when this happens, but you can change this to do
            // whatever you need.
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (TheOrmApplication)
