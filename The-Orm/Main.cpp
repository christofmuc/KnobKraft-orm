/*
   Copyright (c) 2020 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "JuceHeader.h"

#include "MainComponent.h"

#include "Settings.h"
#include "UIModel.h"
#include "Data.h"
#include "OrmLookAndFeel.h"

#include "GenericAdaptation.h"
#include "embedded_module.h"

#include <memory>

#include "version.cpp"

#ifdef USE_SPARKLE
#ifdef WIN32
#include <winsparkle.h>
#endif
#endif

#ifndef _DEBUG
#ifdef USE_SENTRY
#include "sentry.h"
#include "sentry-config.h"

#ifdef LOG_SENTRY
static void print_envelope(sentry_envelope_t *envelope, void *unused_state)
{
	(void)unused_state;
	size_t size_out = 0;
	char *s = sentry_envelope_serialize(envelope, &size_out);
	// As Sentry will still log during shutdown, in this instance we must really check if logging is still a good idea
	if (SimpleLogger::instance()) {
		SimpleLogger::instance()->postMessage("Sentry: " + std::string(s));
	}
	sentry_free(s);
	sentry_envelope_free(envelope);
}
#endif

static void sentryLogger(sentry_level_t level, const char *message, va_list args, void *userdata) {
	ignoreUnused(level, args, userdata);
	char buffer[2048];
	vsnprintf_s(buffer, 2048, message, args);
	// As Sentry will still log during shutdown, in this instance we must really check if logging is still a good idea
	if (SimpleLogger::instance()) {
		SimpleLogger::instance()->postMessage("Sentry: " + std::string(buffer));
	}
}
#endif
#endif

//==============================================================================
class TheOrmApplication  : public JUCEApplication, private ChangeListener
{
public:
    //==============================================================================
	TheOrmApplication() = default;

    const String getApplicationName() override       { return "KnobKraft Orm"; }
	const String getApplicationVersion() override    { return getOrmVersion(); }
    bool moreThanOneInstanceAllowed() override       { return true; }

    //==============================================================================
    void initialise (const String& commandLine) override
    {
		ignoreUnused(commandLine);

		// This method is where you should put your application's initialization code...
		auto applicationDataDirName = "KnobKraftOrm";
		Settings::setSettingsID(applicationDataDirName);

#ifdef USE_SPARKLE
#ifdef WIN32
		// Setup Winsparkle Auto Updater
		win_sparkle_set_app_details(String("KnobKraft").toWideCharPointer(), getApplicationName().toWideCharPointer(), getApplicationVersion().toWideCharPointer());
#endif
#endif

		// Init python for GenericAdaptation
		knobkraft::GenericAdaptation::startupGenericAdaptation();

		// Init python with the embedded pytschirp module, if the Python init was successful
		if (knobkraft::GenericAdaptation::hasPython()) {
			pybind11::gil_scoped_acquire acquire;
			globalImportEmbeddedModules();
		}
		else {
			if (juce::SystemStats::getEnvironmentVariable("ORM_NO_PYTHON", "NOTSET") != "NOTSET") {
				SimpleLogger::instance()->postMessage("Turning off Python integration because environment variable ORM_NO_PYTHON found - you will have less synths!");
			}
			else {
				AlertWindow::showMessageBox(AlertWindow::AlertIconType::WarningIcon, "No Python installation found",
					"No matching version of Python was found on this computer - only native synth implementations will work, adaptations will not be available. "
					"You will have only a limited set of synths supported. Check the log window for more information. If this is ok with you and you want to use "
					"only the build in synths, you can set the environment variable 'set ORM_NO_PYTHON=OK' to suppress this message on startup.");
			}
		}

		// Select colour scheme
		auto lookAndFeel = &LookAndFeel_V4::getDefaultLookAndFeel();
		auto v4 = dynamic_cast<LookAndFeel_V4 *>(lookAndFeel);
		if (v4) {
			v4->setColourScheme(LookAndFeel_V4::getMidnightColourScheme());
		}
		mainWindow = std::make_unique<MainWindow> (getWindowTitle()); 

#ifndef _DEBUG
#ifdef USE_SENTRY
		// Initialize sentry for error crash reporting
		sentry_options_t *options = sentry_options_new();
		std::string dsn = getSentryDSN();
		sentry_options_set_dsn(options, dsn.c_str());
		auto sentryDir = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile(applicationDataDirName).getChildFile("sentry");
		sentry_options_set_database_path(options, sentryDir.getFullPathName().toStdString().c_str());
		std::string releaseName = std::string("KnobKraft Orm Version ") + getOrmVersion();
		sentry_options_set_release(options, releaseName.c_str());
		sentry_options_set_logger(options, sentryLogger, nullptr);
		sentry_options_set_require_user_consent(options, 1);
#ifdef LOG_SENTRY
		sentry_options_set_debug(options, 1);
		sentry_options_set_transport(options, sentry_transport_new(print_envelope));
#endif
		sentry_init(options);

		// Setup user name so I can distinguish my own crashes from the rest. As we never want to user real names, we just generate a random UUID.
		// If you don't like it anymore, you can delete it from the Settings.xml and you'll be a new person!
		std::string userid = Settings::instance().get("UniqueButRandomUserID", juce::Uuid().toDashedString().toStdString());
		Settings::instance().set("UniqueButRandomUserID", userid);
		sentry_value_t user = sentry_value_new_object();
		sentry_value_set_by_key(user, "id", sentry_value_new_string(userid.c_str()));
		sentry_set_user(user);

		// Fire a test event to see if Sentry actually works
		sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_INFO,"custom","Launching KnobKraft Orm"));
#endif
#endif

		// Load Data
		Data::instance().initializeFromSettings();

		// Window Title Refresher
		UIModel::instance()->windowTitle_.addChangeListener(this);
    }

	String getWindowTitle() {
		return getApplicationName() + String(" - Sysex Librarian V" + getOrmVersion());
	}

	void changeListenerCallback(ChangeBroadcaster* source) override
	{
		ignoreUnused(source);
		auto mainComp = dynamic_cast<MainComponent *>(mainWindow->getContentComponent());
		if (mainComp) {
			// This is only called when the window title needs to be changed!
			File currentDatabase(mainComp->getDatabaseFileName());
			mainWindow->setName(getWindowTitle() + " (" + currentDatabase.getFileName() + ")");
		}
	}

    void shutdown() override
    {
		// Unregister
		UIModel::instance()->windowTitle_.removeChangeListener(this);

        // Add your application's shutdown code here...
		SimpleLogger::shutdown(); // That needs to be shutdown before deleting the MainWindow, because it wants to log into that!
		
		// No more Python from here please
		knobkraft::GenericAdaptation::shutdownGenericAdaptation();

		mainWindow = nullptr; // (deletes our window)

		// Save UIModel for next run
		Data::instance().saveToSettings();
		UIModel::shutdown();

		// Shutdown MIDI subsystem after all windows are gone
		midikraft::MidiController::shutdown();

		// Shutdown settings subsystem
		Settings::instance().saveAndClose();
		Settings::shutdown();

#ifndef _DEBUG
#ifdef USE_SENTRY
		// Sentry shutdown
		sentry_shutdown();
#endif
#endif
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
		// Shut down database (that makes a backup)
		// Do this before calling quit
		auto mainComp = dynamic_cast<MainComponent *>(mainWindow->getContentComponent());
		if (mainComp) {
			// Give it a chance to complete the Database backup
			//TODO - should ask user or at least show progress dialog?
			mainComp->shutdown();
		}
		
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
			setResizable(true, true);
            setUsingNativeTitleBar (true);

			// Select colour scheme
			ormLookAndFeel_.setColourScheme(LookAndFeel_V4::getMidnightColourScheme());
			setLookAndFeel(&ormLookAndFeel_);
			
#if JUCE_IOS || JUCE_ANDROID
			setFullScreen(true);
			setContentOwned(new MainComponent(false), false);
			centreWithSize(getWidth(), getHeight());
#else
			if (Settings::instance().keyIsSet("mainWindowSize")) {
				// Restore window size
				restoreWindowStateFromString(Settings::instance().get("mainWindowSize"));
				setContentOwned(new MainComponent(false), false);
			}
			else {
				// Calculate best size for first start. 
				setContentOwned(new MainComponent(true), true);
				setResizable(true, true);
				centreWithSize(getWidth(), getHeight());
			}
#endif
			setVisible(true); //NOLINT

			tooltipGlobalWindow_ = std::make_unique<TooltipWindow>();
        }

		virtual ~MainWindow() override {
			setLookAndFeel(nullptr);
		}

        void closeButtonPressed() override
        {
			Settings::instance().set("mainWindowSize", getWindowStateAsString().toStdString());

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
		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)

		OrmLookAndFeel ormLookAndFeel_;
		std::unique_ptr<TooltipWindow> tooltipGlobalWindow_; //NOLINT
    };


private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (TheOrmApplication)
