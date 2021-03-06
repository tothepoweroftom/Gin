#include <time.h>

//==============================================================================
UpdateChecker::UpdateChecker (ProcessorEditor& editor_)
  : Thread ("Update"), editor (editor_)
{
    if (std::unique_ptr<juce::PropertiesFile> props = editor.slProc.getSettings())
    {
       #ifdef JucePlugin_Name
        juce::String url = props->getValue (JucePlugin_Name "_updateUrl");
        int last   = props->getIntValue (JucePlugin_Name "_lastUpdateCheck");

        if (url.isNotEmpty())
            editor.updateReady (url);
        else if (time (nullptr) > last + 86400)
            startTimer (juce::Random::getSystemRandom().nextInt ({1500, 2500}));
       #endif
    }
}

UpdateChecker::~UpdateChecker()
{
    while (isThreadRunning())
        Thread::sleep (10);
}

void UpdateChecker::timerCallback()
{
    stopTimer();
    startThread();
}

void UpdateChecker::run()
{
   #ifdef JucePlugin_Name
    juce::URL versionsUrl = juce::URL ("https://socalabs.com/version.xml").withParameter ("plugin", JucePlugin_Name).withParameter ("version", JucePlugin_VersionString);
    juce::XmlDocument doc (versionsUrl.readEntireTextStream());
    if (std::unique_ptr<juce::XmlElement> root = doc.getDocumentElement())
    {
        if (std::unique_ptr<juce::PropertiesFile> props = editor.slProc.getSettings())
        {
            props->setValue (JucePlugin_Name "_lastUpdateCheck", int (time (nullptr)));

            auto* child = root->getChildElement (0);
            while (child)
            {
                juce::String name = child->getStringAttribute ("name");
                juce::String ver  = child->getStringAttribute ("num");
                juce::String url  = child->getStringAttribute ("url");

                if (name == JucePlugin_Name && versionStringToInt (ver) > versionStringToInt (JucePlugin_VersionString))
                {
                    props->setValue (JucePlugin_Name "_updateUrl", url);

                    const juce::MessageManagerLock mmLock;
                    editor.updateReady (url);
                    break;
                }

                child = child->getNextElement();
            }
        }
    }
   #endif
}

//==============================================================================
NewsChecker::NewsChecker (ProcessorEditor& editor_)
    : Thread ("News"), editor (editor_)
{
    if (std::unique_ptr<juce::PropertiesFile> props = editor.slProc.getSettings())
    {
        juce::String url = props->getValue ("newsUrl");
        int last   = props->getIntValue ("lastNewsCheck");

        if (url.isNotEmpty())
        {
            editor.newsReady (url);
        }
        else if (time (nullptr) > last + 86400)
        {
            startTimer (juce::Random::getSystemRandom().nextInt ({1500, 2500}));
        }
    }
}

NewsChecker::~NewsChecker()
{
    while (isThreadRunning())
        Thread::sleep (10);
}

void NewsChecker::timerCallback()
{
    stopTimer();
    startThread();
}

void NewsChecker::run()
{
    juce::XmlDocument doc (juce::URL ("https://socalabs.com/feed/").readEntireTextStream());
    if (std::unique_ptr<juce::XmlElement> root = doc.getDocumentElement())
    {
        if (std::unique_ptr<juce::PropertiesFile> props = editor.slProc.getSettings())
        {
            if (auto rss = root->getChildByName ("channel"))
            {
                if (auto item = rss->getChildByName ("item"))
                {
                    if (auto link = item->getChildByName ("link"))
                    {
                        props->setValue ("lastNewsCheck", int (time (nullptr)));

                        juce::String url = link->getAllSubText();

                        juce::StringArray readNews = juce::StringArray::fromTokens (props->getValue ("readNews"), "|", "");
                        if (readNews.isEmpty())
                        {
                            readNews.add (url);
                            props->setValue ("readNews", readNews.joinIntoString ("|"));
                        }

                        if (! readNews.contains (url))
                        {
                            props->setValue ("newsUrl", url);

                            newsUrl = url;
                            triggerAsyncUpdate();
                        }
                    }
                }
            }
        }
    }
}

void NewsChecker::handleAsyncUpdate()
{
    editor.newsReady (newsUrl);
}

//==============================================================================
juce::Rectangle<int> ProcessorEditorBase::getFullGridArea()
{
    return juce::Rectangle<int> (inset, headerHeight + inset, cx * cols + extraWidthPx, cy * rows + extraHeightPx);
}

juce::Rectangle<int> ProcessorEditorBase::getGridArea (int x, int y, int w, int h)
{
    return juce::Rectangle<int> (inset + x * cx, headerHeight + y * cy + inset, w * cx, h * cy);
}

ParamComponent* ProcessorEditorBase::componentForId (const juce::String& uid)
{
    for (auto* c : controls)
    {
        if (c->getUid() == uid)
            return c;
    }
    return nullptr;
}

ParamComponent* ProcessorEditorBase::componentForParam (Parameter& param)
{
    auto uid = param.getUid();
    for (auto c : controls)
    {
        if (c->getUid() == uid)
            return c;
    }
    return nullptr;
}

void ProcessorEditorBase::setGridSize (int x, int y, int extraWidthPx_, int extraHeightPx_)
{
    cols = x;
    rows = y;
    extraWidthPx  = extraWidthPx_;
    extraHeightPx = extraHeightPx_;

    setSize (x * cx + inset * 2 + extraWidthPx,
             y * cy + inset * 2 + headerHeight + extraHeightPx);
}

juce::Rectangle<int> ProcessorEditorBase::getControlsArea()
{
    return getLocalBounds();
}

//==============================================================================
ProcessorEditor::ProcessorEditor (Processor& p, int cx_, int cy_) noexcept
  : ProcessorEditorBase (p, cx_, cy_), slProc (p)
{
    setLookAndFeel (&slProc.lf.get());

    tooltipWindow.setMillisecondsBeforeTipAppears (2000);

    addAndMakeVisible (programs);
    addAndMakeVisible (addButton);
    addAndMakeVisible (deleteButton);
    addAndMakeVisible (nextButton);
    addAndMakeVisible (prevButton);
    addAndMakeVisible (browseButton);
    addAndMakeVisible (helpButton);
    addAndMakeVisible (socaButton);
    addChildComponent (newsButton);
    addChildComponent (updateButton);
    addChildComponent (patchBrowser);

    programs.addListener (this);
    addButton.addListener (this);
    deleteButton.addListener (this);
    nextButton.addListener (this);
    prevButton.addListener (this);
    browseButton.addListener (this);
    helpButton.addListener (this);
    socaButton.addListener (this);
    newsButton.addListener (this);
    updateButton.addListener (this);

    programs.setTooltip ("Select Preset");
    addButton.setTooltip ("Add Preset");
    deleteButton.setTooltip ("Delete Preset");
    browseButton.setTooltip ("Browse Preset");
    nextButton.setTooltip ("Next Preset");
    prevButton.setTooltip ("Prev Preset");
    helpButton.setTooltip ("Help >> About");
    newsButton.setTooltip ("News from SocaLabs");
    socaButton.setTooltip ("Visit www.socalabs.com");
    updateButton.setTooltip ("Update avaliable");

    refreshPrograms();

    updateChecker = std::make_unique<UpdateChecker> (*this);
    newsChecker = std::make_unique<NewsChecker> (*this);

    slProc.addChangeListener (this);
}

ProcessorEditor::~ProcessorEditor()
{
    slProc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void ProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void ProcessorEditor::resized()
{
    ProcessorEditorBase::resized();

    const int pw = 200;
    const int ph = 20;

    programs.setBounds (getWidth() / 2 - pw / 2, 30, pw, ph);

    prevButton.setBounds (programs.getX() - ph - 5, programs.getY(), ph, ph);
    nextButton.setBounds (programs.getRight() + 5, programs.getY(), ph, ph);

    if (hasBrowser)
    {
        browseButton.setBounds (nextButton.getRight() + 15, programs.getY(), ph, ph);
        addButton.setBounds (browseButton.getRight() + 5, programs.getY(), ph, ph);
        deleteButton.setBounds (addButton.getRight() + 5, programs.getY(), ph, ph);
    }
    else
    {
        browseButton.setBounds ({});
        addButton.setBounds (nextButton.getRight() + 15, programs.getY(), ph, ph);
        deleteButton.setBounds (addButton.getRight() + 5, programs.getY(), ph, ph);
    }

    socaButton.setBounds (5, 5, ph, ph);
    newsButton.setBounds (socaButton.getBounds().translated (ph + 5, 0));
    helpButton.setBounds (getWidth() - ph - 5, 5, ph, ph);
    updateButton.setBounds (helpButton.getBounds().translated (- ph - 5, 0));

    patchBrowser.setBounds (getFullGridArea());
}

void ProcessorEditor::refreshPrograms()
{
    programs.clear();

    for (int i = 0; i < processor.getNumPrograms(); i++)
        programs.addItem (processor.getProgramName (i), i + 1);

    programs.setSelectedItemIndex (slProc.getCurrentProgram(), juce::dontSendNotification);
    deleteButton.setEnabled (slProc.getCurrentProgram() != 0);

    patchBrowser.refresh();
}

void ProcessorEditor::buttonClicked (juce::Button* b)
{
    if (b == &nextButton)
    {
        int prog = slProc.getCurrentProgram() + 1;
        if (prog >= slProc.getNumPrograms())
            prog = 0;

        slProc.setCurrentProgram (prog);
    }
    else if (b == &prevButton)
    {
        int prog = slProc.getCurrentProgram() - 1;
        if (prog < 0)
            prog = slProc.getNumPrograms() - 1;

        slProc.setCurrentProgram (prog);
    }
    else if (b == &browseButton)
    {
        b->setToggleState (! b->getToggleState(), juce::dontSendNotification);
        patchBrowser.toFront (false);
        patchBrowser.setVisible (b->getToggleState());
    }
    else if (b == &addButton)
    {
        gin::PluginAlertWindow w ("Create preset:", "", juce::AlertWindow::NoIcon, this);
        w.setLookAndFeel (&slProc.lf.get());
        w.addTextEditor ("name", "", "Name:");

        if (hasBrowser)
        {
            w.addTextEditor ("author", "", "Author:");
            w.addTextEditor ("tags", "", "Tags:");
        }
        
        w.addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w.addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        if (w.runModalLoop (*this) == 1)
        {
            auto txt = juce::File::createLegalFileName (w.getTextEditor ("name")->getText());
            auto aut = (hasBrowser) ? juce::File::createLegalFileName (w.getTextEditor ("author")->getText()) : juce::String();
            auto tag = (hasBrowser) ? juce::File::createLegalFileName (w.getTextEditor ("tags")->getText()) : juce::String();

            if (slProc.hasProgram (txt))
            {
                gin::PluginAlertWindow wc ("Overwrite preset '" + txt + "'?", "", juce::AlertWindow::NoIcon, this);
                wc.addButton ("Yes", 1, juce::KeyPress (juce::KeyPress::returnKey));
                wc.addButton ("No", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                wc.setLookAndFeel (&slProc.lf.get());

                if (wc.runModalLoop (*this) == 0)
                    return;
            }

            if (txt.isNotEmpty())
            {
                slProc.saveProgram (txt, aut, tag);
                refreshPrograms();
            }
        }
    }
    else if (b == &deleteButton)
    {
        gin::PluginAlertWindow w ("Delete preset '" + processor.getProgramName (programs.getSelectedItemIndex()) + "'?", "", juce::AlertWindow::NoIcon, this);
        w.addButton ("Yes", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w.addButton ("No", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w.setLookAndFeel (&slProc.lf.get());

        if (w.runModalLoop (*this))
        {
            slProc.deleteProgram (programs.getSelectedItemIndex());
            refreshPrograms();
        }
    }
    else if (b == &helpButton)
    {
        juce::String msg;

       #ifdef JucePlugin_Name
       #if JUCE_DEBUG
        msg += JucePlugin_Name " v" JucePlugin_VersionString " (" __TIME__ " " __DATE__ ")\n\n";
       #else
        msg += JucePlugin_Name " v" JucePlugin_VersionString " (" __DATE__ ")\n\n";
       #endif
       #endif
        msg += "Roland Rabien\nDavid Rowland\nRAW Material Software JUCE Framework\n";
        if (additionalProgramming.isNotEmpty())
            msg += additionalProgramming;
        msg += "\n";
        msg += "Copyright ";
        msg += juce::String (&__DATE__[7]);

        gin::PluginAlertWindow w ("---- About ----", msg, juce::AlertWindow::NoIcon, this);
        w.addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w.setLookAndFeel (&slProc.lf.get());

        w.runModalLoop (*this);
    }
    else if (b == &updateButton)
    {
       #ifdef JucePlugin_Name
        juce::URL (updateUrl).launchInDefaultBrowser();
        updateButton.setVisible (false);

        if (std::unique_ptr<juce::PropertiesFile> props = slProc.getSettings())
            props->setValue (JucePlugin_Name "_updateUrl", "");
       #endif
    }
    else if (b == &socaButton)
    {
        juce::URL ("http://www.socalabs.com").launchInDefaultBrowser();
    }
    else if (b == &newsButton)
    {
        juce::URL (newsUrl).launchInDefaultBrowser();
        newsButton.setVisible (false);

        if (std::unique_ptr<juce::PropertiesFile> props = slProc.getSettings())
        {
            props->setValue ("newsUrl", "");

            juce::StringArray readNews = juce::StringArray::fromTokens (props->getValue ("readNews"), "|", "");
            readNews.add (newsUrl);
            props->setValue("readNews", readNews.joinIntoString ("|"));
        }
    }
}

void ProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshPrograms();
}

void ProcessorEditor::comboBoxChanged (juce::ComboBox* c)
{
    if (c == &programs)
    {
        int idx = programs.getSelectedItemIndex();
        deleteButton.setEnabled (idx != 0);
        processor.setCurrentProgram (idx);
    }
}

void ProcessorEditor::updateReady (juce::String updateUrl_)
{
    updateUrl = updateUrl_;
    updateButton.setVisible (true);
}

void ProcessorEditor::newsReady (juce::String newsUrl_)
{
    newsUrl = newsUrl_;
    newsButton.setVisible (true);
}
