/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2005 Chris Schoeneman
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "server/InputFilter.h"
#include "server/Server.h"
#include "server/PrimaryClient.h"
#include "barrier/KeyMap.h"
#include "base/EventQueue.h"
#include "base/Log.h"
#include "base/TMethodEventJob.h"

#include <cstdlib>
#include <cstring>

// -----------------------------------------------------------------------------
// Input Filter Condition Classes
// -----------------------------------------------------------------------------
InputFilter::Condition::Condition()
{
    // do nothing
}

InputFilter::Condition::~Condition()
{
    // do nothing
}

void
InputFilter::Condition::enablePrimary(PrimaryClient*)
{
    // do nothing
}

void
InputFilter::Condition::disablePrimary(PrimaryClient*)
{
    // do nothing
}

InputFilter::KeystrokeCondition::KeystrokeCondition(
        IEventQueue* events, IPlatformScreen::KeyInfo* info) :
    m_id(0),
    m_key(info->m_key),
    m_mask(info->m_mask),
    m_events(events)
{
    free(info);
}

InputFilter::KeystrokeCondition::KeystrokeCondition(
        IEventQueue* events, KeyID key, KeyModifierMask mask) :
    m_id(0),
    m_key(key),
    m_mask(mask),
    m_events(events)
{
    // do nothing
}

InputFilter::KeystrokeCondition::~KeystrokeCondition()
{
    // do nothing
}

KeyID
InputFilter::KeystrokeCondition::getKey() const
{
    return m_key;
}

KeyModifierMask
InputFilter::KeystrokeCondition::getMask() const
{
    return m_mask;
}

InputFilter::Condition*
InputFilter::KeystrokeCondition::clone() const
{
    return new KeystrokeCondition(m_events, m_key, m_mask);
}

std::string InputFilter::KeystrokeCondition::format() const
{
    return barrier::string::sprintf("keystroke(%s)",
                            barrier::KeyMap::formatKey(m_key, m_mask).c_str());
}

InputFilter::EFilterStatus
InputFilter::KeystrokeCondition::match(const Event& event)
{
    EFilterStatus status;

    // check for hotkey events
    Event::Type type = event.getType();
    if (type == m_events->forIPrimaryScreen().hotKeyDown()) {
        status = kActivate;
    }
    else if (type == m_events->forIPrimaryScreen().hotKeyUp()) {
        status = kDeactivate;
    }
    else {
        return kNoMatch;
    }

    // check if it's our hotkey
    IPrimaryScreen::HotKeyInfo* kinfo =
        static_cast<IPlatformScreen::HotKeyInfo*>(event.getData());
    if (kinfo->m_id != m_id) {
        return kNoMatch;
    }

    return status;
}

void
InputFilter::KeystrokeCondition::enablePrimary(PrimaryClient* primary)
{
    m_id = primary->registerHotKey(m_key, m_mask);
}

void
InputFilter::KeystrokeCondition::disablePrimary(PrimaryClient* primary)
{
    primary->unregisterHotKey(m_id);
    m_id = 0;
}

InputFilter::MouseButtonCondition::MouseButtonCondition(
        IEventQueue* events, IPlatformScreen::ButtonInfo* info) :
    m_button(info->m_button),
    m_mask(info->m_mask),
    m_events(events)
{
    free(info);
}

InputFilter::MouseButtonCondition::MouseButtonCondition(
        IEventQueue* events, ButtonID button, KeyModifierMask mask) :
    m_button(button),
    m_mask(mask),
    m_events(events)
{
    // do nothing
}

InputFilter::MouseButtonCondition::~MouseButtonCondition()
{
    // do nothing
}

ButtonID
InputFilter::MouseButtonCondition::getButton() const
{
    return m_button;
}

KeyModifierMask
InputFilter::MouseButtonCondition::getMask() const
{
    return m_mask;
}

InputFilter::Condition*
InputFilter::MouseButtonCondition::clone() const
{
    return new MouseButtonCondition(m_events, m_button, m_mask);
}

std::string InputFilter::MouseButtonCondition::format() const
{
    std::string key = barrier::KeyMap::formatKey(kKeyNone, m_mask);
    if (!key.empty()) {
        key += "+";
    }
    return barrier::string::sprintf("mousebutton(%s%d)", key.c_str(), m_button);
}

InputFilter::EFilterStatus
InputFilter::MouseButtonCondition::match(const Event& event)
{
    static const KeyModifierMask s_ignoreMask =
        KeyModifierAltGr | KeyModifierCapsLock |
        KeyModifierNumLock | KeyModifierScrollLock;

    EFilterStatus status;

    // check for hotkey events
    Event::Type type = event.getType();
    if (type == m_events->forIPrimaryScreen().buttonDown()) {
        status = kActivate;
    }
    else if (type == m_events->forIPrimaryScreen().buttonUp()) {
        status = kDeactivate;
    }
    else {
        return kNoMatch;
    }

    // check if it's the right button and modifiers.  ignore modifiers
    // that cannot be combined with a mouse button.
    IPlatformScreen::ButtonInfo* minfo =
        static_cast<IPlatformScreen::ButtonInfo*>(event.getData());
    if (minfo->m_button != m_button ||
        (minfo->m_mask & ~s_ignoreMask) != m_mask) {
        return kNoMatch;
    }

    return status;
}

InputFilter::ScreenConnectedCondition::ScreenConnectedCondition(IEventQueue* events,
                                                                const std::string& screen) :
    m_screen(screen),
    m_events(events)
{
    // do nothing
}

InputFilter::ScreenConnectedCondition::~ScreenConnectedCondition()
{
    // do nothing
}

InputFilter::Condition*
InputFilter::ScreenConnectedCondition::clone() const
{
    return new ScreenConnectedCondition(m_events, m_screen);
}

std::string InputFilter::ScreenConnectedCondition::format() const
{
    return barrier::string::sprintf("connect(%s)", m_screen.c_str());
}

InputFilter::EFilterStatus
InputFilter::ScreenConnectedCondition::match(const Event& event)
{
    if (event.getType() == m_events->forServer().connected()) {
        Server::ScreenConnectedInfo* info =
            static_cast<Server::ScreenConnectedInfo*>(event.getData());
        if (m_screen == info->m_screen || m_screen.empty()) {
            return kActivate;
        }
    }

    return kNoMatch;
}

// -----------------------------------------------------------------------------
// Input Filter Action Classes
// -----------------------------------------------------------------------------
InputFilter::Action::Action()
{
    // do nothing
}

InputFilter::Action::~Action()
{
    // do nothing
}

InputFilter::LockCursorToScreenAction::LockCursorToScreenAction(
        IEventQueue* events, Mode mode) :
    m_mode(mode),
    m_events(events)
{
    // do nothing
}

InputFilter::LockCursorToScreenAction::Mode
InputFilter::LockCursorToScreenAction::getMode() const
{
    return m_mode;
}

InputFilter::Action*
InputFilter::LockCursorToScreenAction::clone() const
{
    return new LockCursorToScreenAction(*this);
}

std::string InputFilter::LockCursorToScreenAction::format() const
{
    static const char* s_mode[] = { "off", "on", "toggle" };

    return barrier::string::sprintf("lockCursorToScreen(%s)", s_mode[m_mode]);
}

void
InputFilter::LockCursorToScreenAction::perform(const Event& event)
{
    static const Server::LockCursorToScreenInfo::State s_state[] = {
        Server::LockCursorToScreenInfo::kOff,
        Server::LockCursorToScreenInfo::kOn,
        Server::LockCursorToScreenInfo::kToggle
    };

    // send event
    Server::LockCursorToScreenInfo* info =
        Server::LockCursorToScreenInfo::alloc(s_state[m_mode]);
    m_events->addEvent(Event(m_events->forServer().lockCursorToScreen(),
                                event.getTarget(), info,
                                Event::kDeliverImmediately));
}

InputFilter::SwitchToScreenAction::SwitchToScreenAction(IEventQueue* events,
                                                        const std::string& screen) :
    m_screen(screen),
    m_events(events)
{
    // do nothing
}

std::string InputFilter::SwitchToScreenAction::getScreen() const
{
    return m_screen;
}

InputFilter::Action*
InputFilter::SwitchToScreenAction::clone() const
{
    return new SwitchToScreenAction(*this);
}

std::string InputFilter::SwitchToScreenAction::format() const
{
    return barrier::string::sprintf("switchToScreen(%s)", m_screen.c_str());
}

void
InputFilter::SwitchToScreenAction::perform(const Event& event)
{
    // pick screen name.  if m_screen is empty then use the screen from
    // event if it has one.
    std::string screen = m_screen;
    if (screen.empty() && event.getType() == m_events->forServer().connected()) {
        Server::ScreenConnectedInfo* info =
            static_cast<Server::ScreenConnectedInfo*>(event.getData());
        screen = info->m_screen;
    }

    // send event
    Server::SwitchToScreenInfo* info =
        Server::SwitchToScreenInfo::alloc(screen);
    m_events->addEvent(Event(m_events->forServer().switchToScreen(),
                                event.getTarget(), info,
                                Event::kDeliverImmediately));
}

InputFilter::ToggleScreenAction::ToggleScreenAction(IEventQueue* events) :
    m_events(events)
{
    // do nothing
}

InputFilter::Action*
InputFilter::ToggleScreenAction::clone() const
{
    return new ToggleScreenAction(*this);
}

String
InputFilter::ToggleScreenAction::format() const
{
    return barrier::string::sprintf("toggleScreen");
}

void
InputFilter::ToggleScreenAction::perform(const Event& event)
{
    m_events->addEvent(Event(m_events->forServer().toggleScreen(),
                             event.getTarget(), NULL,
                             Event::kDeliverImmediately));
}

InputFilter::SwitchInDirectionAction::SwitchInDirectionAction(
        IEventQueue* events, EDirection direction) :
    m_direction(direction),
    m_events(events)
{
    // do nothing
}

EDirection
InputFilter::SwitchInDirectionAction::getDirection() const
{
    return m_direction;
}

InputFilter::Action*
InputFilter::SwitchInDirectionAction::clone() const
{
    return new SwitchInDirectionAction(*this);
}

std::string InputFilter::SwitchInDirectionAction::format() const
{
    static const char* s_names[] = {
        "",
        "left",
        "right",
        "up",
        "down"
    };

    return barrier::string::sprintf("switchInDirection(%s)", s_names[m_direction]);
}

void
InputFilter::SwitchInDirectionAction::perform(const Event& event)
{
    Server::SwitchInDirectionInfo* info =
        Server::SwitchInDirectionInfo::alloc(m_direction);
    m_events->addEvent(Event(m_events->forServer().switchInDirection(),
                                event.getTarget(), info,
                                Event::kDeliverImmediately));
}

InputFilter::KeyboardBroadcastAction::KeyboardBroadcastAction(
        IEventQueue* events, Mode mode) :
    m_mode(mode),
    m_events(events)
{
    // do nothing
}

InputFilter::KeyboardBroadcastAction::KeyboardBroadcastAction(IEventQueue* events, Mode mode,
                                                              const std::set<std::string>& screens) :
    m_mode(mode),
    m_screens(IKeyState::KeyInfo::join(screens)),
    m_events(events)
{
    // do nothing
}

InputFilter::KeyboardBroadcastAction::Mode
InputFilter::KeyboardBroadcastAction::getMode() const
{
    return m_mode;
}

std::set<std::string> InputFilter::KeyboardBroadcastAction::getScreens() const
{
    std::set<std::string> screens;
    IKeyState::KeyInfo::split(m_screens.c_str(), screens);
    return screens;
}

InputFilter::Action*
InputFilter::KeyboardBroadcastAction::clone() const
{
    return new KeyboardBroadcastAction(*this);
}

std::string InputFilter::KeyboardBroadcastAction::format() const
{
    static const char* s_mode[] = { "off", "on", "toggle" };
    static const char* s_name = "keyboardBroadcast";

    if (m_screens.empty() || m_screens[0] == '*') {
        return barrier::string::sprintf("%s(%s)", s_name, s_mode[m_mode]);
    }
    else {
        return barrier::string::sprintf("%s(%s,%.*s)", s_name, s_mode[m_mode],
                            m_screens.size() - 2,
                            m_screens.c_str() + 1);
    }
}

void
InputFilter::KeyboardBroadcastAction::perform(const Event& event)
{
    static const Server::KeyboardBroadcastInfo::State s_state[] = {
        Server::KeyboardBroadcastInfo::kOff,
        Server::KeyboardBroadcastInfo::kOn,
        Server::KeyboardBroadcastInfo::kToggle
    };

    // send event
    Server::KeyboardBroadcastInfo* info =
        Server::KeyboardBroadcastInfo::alloc(s_state[m_mode], m_screens);
    m_events->addEvent(Event(m_events->forServer().keyboardBroadcast(),
                                event.getTarget(), info,
                                Event::kDeliverImmediately));
}

InputFilter::KeystrokeAction::KeystrokeAction(
        IEventQueue* events, IPlatformScreen::KeyInfo* info, bool press) :
    m_keyInfo(info),
    m_press(press),
    m_events(events)
{
    // do nothing
}

InputFilter::KeystrokeAction::~KeystrokeAction()
{
    free(m_keyInfo);
}

void
InputFilter::KeystrokeAction::adoptInfo(IPlatformScreen::KeyInfo* info)
{
    free(m_keyInfo);
    m_keyInfo = info;
}

const IPlatformScreen::KeyInfo*
InputFilter::KeystrokeAction::getInfo() const
{
    return m_keyInfo;
}

bool
InputFilter::KeystrokeAction::isOnPress() const
{
    return m_press;
}

InputFilter::Action*
InputFilter::KeystrokeAction::clone() const
{
    IKeyState::KeyInfo* info = IKeyState::KeyInfo::alloc(*m_keyInfo);
    return new KeystrokeAction(m_events, info, m_press);
}

std::string InputFilter::KeystrokeAction::format() const
{
    const char* type = formatName();

    if (m_keyInfo->m_screens[0] == '\0') {
        return barrier::string::sprintf("%s(%s)", type,
                            barrier::KeyMap::formatKey(m_keyInfo->m_key,
                                m_keyInfo->m_mask).c_str());
    }
    else if (m_keyInfo->m_screens[0] == '*') {
        return barrier::string::sprintf("%s(%s,*)", type,
                            barrier::KeyMap::formatKey(m_keyInfo->m_key,
                                m_keyInfo->m_mask).c_str());
    }
    else {
        return barrier::string::sprintf("%s(%s,%.*s)", type,
                            barrier::KeyMap::formatKey(m_keyInfo->m_key,
                                m_keyInfo->m_mask).c_str(),
                            strnlen(m_keyInfo->m_screens + 1, SIZE_MAX) - 1,
                            m_keyInfo->m_screens + 1);
    }
}

void
InputFilter::KeystrokeAction::perform(const Event& event)
{
    Event::Type type = m_press ?
        m_events->forIKeyState().keyDown() :
        m_events->forIKeyState().keyUp();

    m_events->addEvent(Event(m_events->forIPrimaryScreen().fakeInputBegin(),
                                event.getTarget(), NULL,
                                Event::kDeliverImmediately));
    m_events->addEvent(Event(type, event.getTarget(), m_keyInfo,
                                Event::kDeliverImmediately |
                                Event::kDontFreeData));
    m_events->addEvent(Event(m_events->forIPrimaryScreen().fakeInputEnd(),
                                event.getTarget(), NULL,
                                Event::kDeliverImmediately));
}

const char*
InputFilter::KeystrokeAction::formatName() const
{
    return (m_press ? "keyDown" : "keyUp");
}

InputFilter::MouseButtonAction::MouseButtonAction(
        IEventQueue* events, IPlatformScreen::ButtonInfo* info, bool press) :
    m_buttonInfo(info),
    m_press(press),
    m_events(events)
{
    // do nothing
}

InputFilter::MouseButtonAction::~MouseButtonAction()
{
    free(m_buttonInfo);
}

const IPlatformScreen::ButtonInfo*
InputFilter::MouseButtonAction::getInfo() const
{
    return m_buttonInfo;
}

bool
InputFilter::MouseButtonAction::isOnPress() const
{
    return m_press;
}

InputFilter::Action*
InputFilter::MouseButtonAction::clone() const
{
    IPlatformScreen::ButtonInfo* info =
        IPrimaryScreen::ButtonInfo::alloc(*m_buttonInfo);
    return new MouseButtonAction(m_events, info, m_press);
}

std::string InputFilter::MouseButtonAction::format() const
{
    const char* type = formatName();

    std::string key = barrier::KeyMap::formatKey(kKeyNone, m_buttonInfo->m_mask);
    return barrier::string::sprintf("%s(%s%s%d)", type,
                            key.c_str(), key.empty() ? "" : "+",
                            m_buttonInfo->m_button);
}

void
InputFilter::MouseButtonAction::perform(const Event& event)

{
    // send modifiers
    IPlatformScreen::KeyInfo* modifierInfo = NULL;
    if (m_buttonInfo->m_mask != 0) {
        KeyID key = m_press ? kKeySetModifiers : kKeyClearModifiers;
        modifierInfo =
            IKeyState::KeyInfo::alloc(key, m_buttonInfo->m_mask, 0, 1);
        m_events->addEvent(Event(m_events->forIKeyState().keyDown(),
                                event.getTarget(), modifierInfo,
                                Event::kDeliverImmediately));
    }

    // send button
    Event::Type type = m_press ? m_events->forIPrimaryScreen().buttonDown() :
                                m_events->forIPrimaryScreen().buttonUp();
    m_events->addEvent(Event(type, event.getTarget(), m_buttonInfo,
                                Event::kDeliverImmediately |
                                Event::kDontFreeData));
}

const char*
InputFilter::MouseButtonAction::formatName() const
{
    return (m_press ? "mouseDown" : "mouseUp");
}


InputFilter::UserScriptAction::UserScriptAction(IEventQueue* events,
        const std::string& scriptCommand) :
    m_scriptCommand(scriptCommand),
    m_events(events)
{
    // do nothing
}

std::string InputFilter::UserScriptAction::getScriptCommand() const
{
    return m_scriptCommand;
}

InputFilter::Action*
InputFilter::UserScriptAction::clone() const
{
    return new UserScriptAction(*this);
}

std::string InputFilter::UserScriptAction::format() const
{
    return barrier::string::sprintf("userScript(%s)", m_scriptCommand.c_str());
}

void
InputFilter::UserScriptAction::perform(const Event& event)
{
    // pick screen name.  if m_screen is empty then use the screen from
    // event if it has one.
    std::string scriptCommand = m_scriptCommand;
    if (scriptCommand.empty()) {
        LOG((CLOG_ERR "script ID empty"));
        return;
    }

    // exec script
#if !defined(_WIN32)
    std::string s = barrier::string::sprintf(R"(sh -c '%s')", scriptCommand.c_str());
#else
    std::string s = barrier::string::sprintf(R"(cmd.exe /C "%s")", scriptCommand.c_str());

#endif
    LOG((CLOG_DEBUG "%s",s.c_str()));
    system(s.c_str());
}

//
// InputFilter::Rule
//

InputFilter::Rule::Rule() :
    m_condition(NULL)
{
    // do nothing
}

InputFilter::Rule::Rule(Condition* adoptedCondition) :
    m_condition(adoptedCondition)
{
    // do nothing
}

InputFilter::Rule::Rule(const Rule& rule) :
    m_condition(NULL)
{
    copy(rule);
}

InputFilter::Rule::~Rule()
{
    clear();
}

InputFilter::Rule&
InputFilter::Rule::operator=(const Rule& rule)
{
    if (&rule != this) {
        copy(rule);
    }
    return *this;
}

void
InputFilter::Rule::clear()
{
    delete m_condition;
    for (ActionList::iterator i = m_activateActions.begin();
                                i != m_activateActions.end(); ++i) {
        delete *i;
    }
    for (ActionList::iterator i = m_deactivateActions.begin();
                                i != m_deactivateActions.end(); ++i) {
        delete *i;
    }

    m_condition = NULL;
    m_activateActions.clear();
    m_deactivateActions.clear();
}

void
InputFilter::Rule::copy(const Rule& rule)
{
    clear();
    if (rule.m_condition != NULL) {
        m_condition = rule.m_condition->clone();
    }
    for (ActionList::const_iterator i = rule.m_activateActions.begin();
                                i != rule.m_activateActions.end(); ++i) {
        m_activateActions.push_back((*i)->clone());
    }
    for (ActionList::const_iterator i = rule.m_deactivateActions.begin();
                                i != rule.m_deactivateActions.end(); ++i) {
        m_deactivateActions.push_back((*i)->clone());
    }
}

void
InputFilter::Rule::setCondition(Condition* adopted)
{
    delete m_condition;
    m_condition = adopted;
}

void
InputFilter::Rule::adoptAction(Action* action, bool onActivation)
{
    if (action != NULL) {
        if (onActivation) {
            m_activateActions.push_back(action);
        }
        else {
            m_deactivateActions.push_back(action);
        }
    }
}

void
InputFilter::Rule::removeAction(bool onActivation, UInt32 index)
{
    if (onActivation) {
        delete m_activateActions[index];
        m_activateActions.erase(m_activateActions.begin() + index);
    }
    else {
        delete m_deactivateActions[index];
        m_deactivateActions.erase(m_deactivateActions.begin() + index);
    }
}

void
InputFilter::Rule::replaceAction(Action* adopted,
                bool onActivation, UInt32 index)
{
    if (adopted == NULL) {
        removeAction(onActivation, index);
    }
    else if (onActivation) {
        delete m_activateActions[index];
        m_activateActions[index] = adopted;
    }
    else {
        delete m_deactivateActions[index];
        m_deactivateActions[index] = adopted;
    }
}

void
InputFilter::Rule::enable(PrimaryClient* primaryClient)
{
    if (m_condition != NULL) {
        m_condition->enablePrimary(primaryClient);
    }
}

void
InputFilter::Rule::disable(PrimaryClient* primaryClient)
{
    if (m_condition != NULL) {
        m_condition->disablePrimary(primaryClient);
    }
}

bool
InputFilter::Rule::handleEvent(const Event& event)
{
    // NULL condition never matches
    if (m_condition == NULL) {
        return false;
    }

    // match
    const ActionList* actions;
    switch (m_condition->match(event)) {
    default:
        // not handled
        return false;

    case kActivate:
        actions = &m_activateActions;
        LOG((CLOG_DEBUG1 "activate actions"));
        break;

    case kDeactivate:
        actions = &m_deactivateActions;
        LOG((CLOG_DEBUG1 "deactivate actions"));
        break;
    }

    // perform actions
    for (ActionList::const_iterator i = actions->begin();
                                i != actions->end(); ++i) {
        LOG((CLOG_DEBUG1 "hotkey: %s", (*i)->format().c_str()));
        (*i)->perform(event);
    }

    return true;
}

std::string InputFilter::Rule::format() const
{
    std::string s;
    if (m_condition != NULL) {
        // condition
        s += m_condition->format();
        s += " = ";

        // activate actions
        ActionList::const_iterator i = m_activateActions.begin();
        if (i != m_activateActions.end()) {
            s += (*i)->format();
            while (++i != m_activateActions.end()) {
                s += ", ";
                s += (*i)->format();
            }
        }

        // deactivate actions
        if (!m_deactivateActions.empty()) {
            s += "; ";
            i = m_deactivateActions.begin();
            if (i != m_deactivateActions.end()) {
                s += (*i)->format();
                while (++i != m_deactivateActions.end()) {
                    s += ", ";
                    s += (*i)->format();
                }
            }
        }
    }
    return s;
}

const InputFilter::Condition*
InputFilter::Rule::getCondition() const
{
    return m_condition;
}

UInt32
InputFilter::Rule::getNumActions(bool onActivation) const
{
    if (onActivation) {
        return static_cast<UInt32>(m_activateActions.size());
    }
    else {
        return static_cast<UInt32>(m_deactivateActions.size());
    }
}

const InputFilter::Action&
InputFilter::Rule::getAction(bool onActivation, UInt32 index) const
{
    if (onActivation) {
        return *m_activateActions[index];
    }
    else {
        return *m_deactivateActions[index];
    }
}


// -----------------------------------------------------------------------------
// Input Filter Class
// -----------------------------------------------------------------------------
InputFilter::InputFilter(IEventQueue* events) :
    m_primaryClient(NULL),
    m_events(events)
{
    // do nothing
}

InputFilter::InputFilter(const InputFilter& x) :
    m_ruleList(x.m_ruleList),
    m_primaryClient(NULL),
    m_events(x.m_events)
{
    setPrimaryClient(x.m_primaryClient);
}

InputFilter::~InputFilter()
{
    setPrimaryClient(NULL);
}

InputFilter&
InputFilter::operator=(const InputFilter& x)
{
    if (&x != this) {
        PrimaryClient* oldClient = m_primaryClient;
        setPrimaryClient(NULL);

        m_ruleList = x.m_ruleList;

        setPrimaryClient(oldClient);
    }
    return *this;
}

void
InputFilter::addFilterRule(const Rule& rule)
{
    m_ruleList.push_back(rule);
    if (m_primaryClient != NULL) {
        m_ruleList.back().enable(m_primaryClient);
    }
}

void
InputFilter::removeFilterRule(UInt32 index)
{
    if (m_primaryClient != NULL) {
        m_ruleList[index].disable(m_primaryClient);
    }
    m_ruleList.erase(m_ruleList.begin() + index);
}

InputFilter::Rule&
InputFilter::getRule(UInt32 index)
{
    return m_ruleList[index];
}

void
InputFilter::setPrimaryClient(PrimaryClient* client)
{
    if (m_primaryClient == client) {
        return;
    }

    if (m_primaryClient != NULL) {
        for (RuleList::iterator rule  = m_ruleList.begin();
                                 rule != m_ruleList.end(); ++rule) {
            rule->disable(m_primaryClient);
        }

        m_events->removeHandler(m_events->forIKeyState().keyDown(),
                            m_primaryClient->getEventTarget());
        m_events->removeHandler(m_events->forIKeyState().keyUp(),
                            m_primaryClient->getEventTarget());
        m_events->removeHandler(m_events->forIKeyState().keyRepeat(),
                            m_primaryClient->getEventTarget());
        m_events->removeHandler(m_events->forIPrimaryScreen().buttonDown(),
                            m_primaryClient->getEventTarget());
        m_events->removeHandler(m_events->forIPrimaryScreen().buttonUp(),
                            m_primaryClient->getEventTarget());
        m_events->removeHandler(m_events->forIPrimaryScreen().hotKeyDown(),
                            m_primaryClient->getEventTarget());
        m_events->removeHandler(m_events->forIPrimaryScreen().hotKeyUp(),
                            m_primaryClient->getEventTarget());
        m_events->removeHandler(m_events->forServer().connected(),
                            m_primaryClient->getEventTarget());
    }

    m_primaryClient = client;

    if (m_primaryClient != NULL) {
        m_events->adoptHandler(m_events->forIKeyState().keyDown(),
                            m_primaryClient->getEventTarget(),
                            new TMethodEventJob<InputFilter>(this,
                                &InputFilter::handleEvent));
        m_events->adoptHandler(m_events->forIKeyState().keyUp(),
                            m_primaryClient->getEventTarget(),
                            new TMethodEventJob<InputFilter>(this,
                                &InputFilter::handleEvent));
        m_events->adoptHandler(m_events->forIKeyState().keyRepeat(),
                            m_primaryClient->getEventTarget(),
                            new TMethodEventJob<InputFilter>(this,
                                &InputFilter::handleEvent));
        m_events->adoptHandler(m_events->forIPrimaryScreen().buttonDown(),
                            m_primaryClient->getEventTarget(),
                            new TMethodEventJob<InputFilter>(this,
                                &InputFilter::handleEvent));
        m_events->adoptHandler(m_events->forIPrimaryScreen().buttonUp(),
                            m_primaryClient->getEventTarget(),
                            new TMethodEventJob<InputFilter>(this,
                                &InputFilter::handleEvent));
        m_events->adoptHandler(m_events->forIPrimaryScreen().hotKeyDown(),
                            m_primaryClient->getEventTarget(),
                            new TMethodEventJob<InputFilter>(this,
                                &InputFilter::handleEvent));
        m_events->adoptHandler(m_events->forIPrimaryScreen().hotKeyUp(),
                            m_primaryClient->getEventTarget(),
                            new TMethodEventJob<InputFilter>(this,
                                &InputFilter::handleEvent));
        m_events->adoptHandler(m_events->forServer().connected(),
                            m_primaryClient->getEventTarget(),
                            new TMethodEventJob<InputFilter>(this,
                                &InputFilter::handleEvent));

        for (RuleList::iterator rule  = m_ruleList.begin();
                                 rule != m_ruleList.end(); ++rule) {
            rule->enable(m_primaryClient);
        }
    }
}

std::string InputFilter::format(const std::string& linePrefix) const
{
    std::string s;
    for (RuleList::const_iterator i = m_ruleList.begin();
                                i != m_ruleList.end(); ++i) {
        s += linePrefix;
        s += i->format();
        s += "\n";
    }
    return s;
}

UInt32
InputFilter::getNumRules() const
{
    return static_cast<UInt32>(m_ruleList.size());
}

bool
InputFilter::operator==(const InputFilter& x) const
{
    // if there are different numbers of rules then we can't be equal
    if (m_ruleList.size() != x.m_ruleList.size()) {
        return false;
    }

    // compare rule lists.  the easiest way to do that is to format each
    // rule into a string, sort the strings, then compare the results.
    std::vector<std::string> aList, bList;
    for (RuleList::const_iterator i = m_ruleList.begin();
                                i != m_ruleList.end(); ++i) {
        aList.push_back(i->format());
    }
    for (RuleList::const_iterator i = x.m_ruleList.begin();
                                i != x.m_ruleList.end(); ++i) {
        bList.push_back(i->format());
    }
    std::partial_sort(aList.begin(), aList.end(), aList.end());
    std::partial_sort(bList.begin(), bList.end(), bList.end());
    return (aList == bList);
}

bool
InputFilter::operator!=(const InputFilter& x) const
{
    return !operator==(x);
}

void
InputFilter::handleEvent(const Event& event, void*)
{
    // copy event and adjust target
    Event myEvent(event.getType(), this, event.getData(),
                                event.getFlags() | Event::kDontFreeData |
                                Event::kDeliverImmediately);

    // let each rule try to match the event until one does
    for (RuleList::iterator rule  = m_ruleList.begin();
                             rule != m_ruleList.end(); ++rule) {
        if (rule->handleEvent(myEvent)) {
            // handled
            return;
        }
    }

    // not handled so pass through
    m_events->addEvent(myEvent);
}
