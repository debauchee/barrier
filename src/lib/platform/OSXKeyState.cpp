/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2004 Chris Schoeneman
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

#include "platform/OSXKeyState.h"
#include "platform/OSXUchrKeyResource.h"
#include "platform/OSXMediaKeySupport.h"
#include "arch/Arch.h"
#include "base/Log.h"

#include <Carbon/Carbon.h>
#include <IOKit/hidsystem/IOHIDLib.h>

// Note that some virtual keys codes appear more than once.  The
// first instance of a virtual key code maps to the KeyID that we
// want to generate for that code.  The others are for mapping
// different KeyIDs to a single key code.
static const UInt32 s_shiftVK    = kVK_Shift;
static const UInt32 s_controlVK  = kVK_Control;
static const UInt32 s_altVK      = kVK_Option;
static const UInt32 s_superVK    = kVK_Command;
static const UInt32 s_capsLockVK = kVK_CapsLock;
static const UInt32 s_numLockVK  = kVK_ANSI_KeypadClear; // 71

static const UInt32 s_brightnessUp = 144;
static const UInt32 s_brightnessDown = 145;
static const UInt32 s_missionControlVK = 160;
static const UInt32 s_launchpadVK = 131;

static const UInt32 s_osxNumLock = 1 << 16;

static const UInt32 s_int4VK = 0x8a; // international4
static const UInt32 s_int5VK = 0x8b; // international5

struct KeyEntry {
public:
    KeyID                m_keyID;
    UInt32                m_virtualKey;
};
static const KeyEntry    s_controlKeys[] = {
    // cursor keys.  if we don't do this we'll may still get these from
    // the keyboard resource but they may not correspond to the arrow
    // keys.
    { kKeyLeft,        kVK_LeftArrow },
    { kKeyRight,        kVK_RightArrow },
    { kKeyUp,        kVK_UpArrow },
    { kKeyDown,        kVK_DownArrow },
    { kKeyHome,        kVK_Home },
    { kKeyEnd,        kVK_End },
    { kKeyPageUp,        kVK_PageUp },
    { kKeyPageDown,        kVK_PageDown },
    { kKeyInsert,        kVK_Help }, // Mac Keyboards have 'Help' on 'Insert'

    // function keys
    { kKeyF1,        kVK_F1 },
    { kKeyF2,        kVK_F2 },
    { kKeyF3,        kVK_F3 },
    { kKeyF4,        kVK_F4 },
    { kKeyF5,        kVK_F5 },
    { kKeyF6,        kVK_F6 },
    { kKeyF7,        kVK_F7 },
    { kKeyF8,        kVK_F8 },
    { kKeyF9,        kVK_F9 },
    { kKeyF10,        kVK_F10 },
    { kKeyF11,        kVK_F11 },
    { kKeyF12,        kVK_F12 },
    { kKeyF13,        kVK_F13 },
    { kKeyF14,        kVK_F14 },
    { kKeyF15,        kVK_F15 },
    { kKeyF16,        kVK_F16 },

    { kKeyKP_0,        kVK_ANSI_Keypad0 },
    { kKeyKP_1,        kVK_ANSI_Keypad1 },
    { kKeyKP_2,        kVK_ANSI_Keypad2 },
    { kKeyKP_3,        kVK_ANSI_Keypad3 },
    { kKeyKP_4,        kVK_ANSI_Keypad4 },
    { kKeyKP_5,        kVK_ANSI_Keypad5 },
    { kKeyKP_6,        kVK_ANSI_Keypad6 },
    { kKeyKP_7,        kVK_ANSI_Keypad7 },
    { kKeyKP_8,        kVK_ANSI_Keypad8 },
    { kKeyKP_9,        kVK_ANSI_Keypad9 },
    { kKeyKP_Decimal,    kVK_ANSI_KeypadDecimal },
    { kKeyKP_Equal,        kVK_ANSI_KeypadEquals },
    { kKeyKP_Multiply,    kVK_ANSI_KeypadMultiply },
    { kKeyKP_Add,        kVK_ANSI_KeypadPlus },
    { kKeyKP_Divide,    kVK_ANSI_KeypadDivide },
    { kKeyKP_Subtract,    kVK_ANSI_KeypadMinus },
    { kKeyKP_Enter,        kVK_ANSI_KeypadEnter },

    // virtual key 110 is fn+enter and i have no idea what that's supposed
    // to map to.  also the enter key with numlock on is a modifier but i
    // don't know which.

    // modifier keys.  OS X doesn't seem to support right handed versions
    // of modifier keys so we map them to the left handed versions.
    { kKeyShift_L,        s_shiftVK },
    { kKeyShift_R,        s_shiftVK }, // 60
    { kKeyControl_L,    s_controlVK },
    { kKeyControl_R,    s_controlVK }, // 62
    { kKeyAlt_L,        s_altVK },
    { kKeyAlt_R,        s_altVK },
    { kKeySuper_L,        s_superVK },
    { kKeySuper_R,        s_superVK }, // 61
    { kKeyMeta_L,        s_superVK },
    { kKeyMeta_R,        s_superVK }, // 61

    // toggle modifiers
    { kKeyNumLock,        s_numLockVK },
    { kKeyCapsLock,        s_capsLockVK },

    { kKeyMissionControl, s_missionControlVK },
    { kKeyLaunchpad, s_launchpadVK },
    { kKeyBrightnessUp,  s_brightnessUp },
    { kKeyBrightnessDown, s_brightnessDown },

    // JIS keyboards only
    { kKeyEisuToggle, kVK_JIS_Eisu },
    { kKeyKana, kVK_JIS_Kana },
    { kKeyMuhenkan, s_int5VK },
    { kKeyHenkan, s_int4VK },
    { kKeyZenkaku, kVK_ANSI_Grave }
};


//
// OSXKeyState
//

OSXKeyState::OSXKeyState(IEventQueue* events) :
    KeyState(events)
{
    init();
}

OSXKeyState::OSXKeyState(IEventQueue* events, barrier::KeyMap& keyMap) :
    KeyState(events, keyMap)
{
    init();
}

OSXKeyState::~OSXKeyState()
{
}

void
OSXKeyState::init()
{
    m_deadKeyState = 0;
    m_shiftPressed = false;
    m_controlPressed = false;
    m_altPressed = false;
    m_superPressed = false;
    m_capsPressed = false;

    // build virtual key map
    for (size_t i = 0; i < sizeof(s_controlKeys) / sizeof(s_controlKeys[0]);
        ++i) {

        m_virtualKeyMap[s_controlKeys[i].m_virtualKey] =
            s_controlKeys[i].m_keyID;
    }
}

KeyModifierMask
OSXKeyState::mapModifiersFromOSX(UInt32 mask) const
{
    KeyModifierMask outMask = 0;
    if ((mask & kCGEventFlagMaskShift) != 0) {
        outMask |= KeyModifierShift;
    }
    if ((mask & kCGEventFlagMaskControl) != 0) {
        outMask |= KeyModifierControl;
    }
    if ((mask & kCGEventFlagMaskAlternate) != 0) {
        outMask |= KeyModifierAlt;
    }
    if ((mask & kCGEventFlagMaskCommand) != 0) {
        outMask |= KeyModifierSuper;
    }
    if ((mask & kCGEventFlagMaskAlphaShift) != 0) {
        outMask |= KeyModifierCapsLock;
    }
    if ((mask & kCGEventFlagMaskNumericPad) != 0) {
        outMask |= KeyModifierNumLock;
    }

    LOG((CLOG_DEBUG1 "mask=%04x outMask=%04x", mask, outMask));
    return outMask;
}

KeyModifierMask
OSXKeyState::mapModifiersToCarbon(UInt32 mask) const
{
    KeyModifierMask outMask = 0;
    if ((mask & kCGEventFlagMaskShift) != 0) {
        outMask |= shiftKey;
    }
    if ((mask & kCGEventFlagMaskControl) != 0) {
        outMask |= controlKey;
    }
    if ((mask & kCGEventFlagMaskCommand) != 0) {
        outMask |= cmdKey;
    }
    if ((mask & kCGEventFlagMaskAlternate) != 0) {
        outMask |= optionKey;
    }
    if ((mask & kCGEventFlagMaskAlphaShift) != 0) {
        outMask |= alphaLock;
    }
    if ((mask & kCGEventFlagMaskNumericPad) != 0) {
        outMask |= s_osxNumLock;
    }

    return outMask;
}

KeyButton
OSXKeyState::mapKeyFromEvent(KeyIDs& ids,
                KeyModifierMask* maskOut, CGEventRef event) const
{
    ids.clear();

    // map modifier key
    if (maskOut != NULL) {
        KeyModifierMask activeMask = getActiveModifiers();
        activeMask &= ~KeyModifierAltGr;
        *maskOut    = activeMask;
    }

    // get virtual key
    UInt32 vkCode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

    // handle up events
    UInt32 eventKind = CGEventGetType(event);
    if (eventKind == kCGEventKeyUp) {
        // the id isn't used.  we just need the same button we used on
        // the key press.  note that we don't use or reset the dead key
        // state;  up events should not affect the dead key state.
        ids.push_back(kKeyNone);
        return mapVirtualKeyToKeyButton(vkCode);
    }

    // check for special keys
    VirtualKeyMap::const_iterator i = m_virtualKeyMap.find(vkCode);
    if (i != m_virtualKeyMap.end()) {
        m_deadKeyState = 0;
        ids.push_back(i->second);
        return mapVirtualKeyToKeyButton(vkCode);
    }

    // get keyboard info
    TISInputSourceRef currentKeyboardLayout = TISCopyCurrentKeyboardLayoutInputSource();

    if (currentKeyboardLayout == NULL) {
        return kKeyNone;
    }

    // get the event modifiers and remove the command and control
    // keys.  note if we used them though.
    // UCKeyTranslate expects old-style Carbon modifiers, so convert.
    UInt32 modifiers;
    modifiers = mapModifiersToCarbon(CGEventGetFlags(event));
    static const UInt32 s_commandModifiers =
        cmdKey | controlKey | rightControlKey;
    bool isCommand = ((modifiers & s_commandModifiers) != 0);
    modifiers &= ~s_commandModifiers;

    // if we've used a command key then we want the glyph produced without
    // the option key (i.e. the base glyph).
    //if (isCommand) {
        modifiers &= ~optionKey;
    //}

    // choose action
    UInt16 action;
    if (eventKind==kCGEventKeyDown) {
        action = kUCKeyActionDown;
    }
    else if (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat)==1) {
        action = kUCKeyActionAutoKey;
    }
    else {
        return 0;
    }

    // translate via uchr resource
    CFDataRef ref = (CFDataRef) TISGetInputSourceProperty(currentKeyboardLayout,
                                kTISPropertyUnicodeKeyLayoutData);
    const UCKeyboardLayout* layout = (const UCKeyboardLayout*) CFDataGetBytePtr(ref);
    const bool layoutValid = (layout != NULL);

    if (layoutValid) {
        // translate key
        UniCharCount count;
        UniChar chars[2];
        LOG((CLOG_DEBUG2 "modifiers: %08x", modifiers & 0xffu));
        OSStatus status = UCKeyTranslate(layout,
                            vkCode & 0xffu, action,
                            (modifiers >> 8) & 0xffu,
                            LMGetKbdType(), 0, &m_deadKeyState,
                            sizeof(chars) / sizeof(chars[0]), &count, chars);

        // get the characters
        if (status == 0) {
            if (count != 0 || m_deadKeyState == 0) {
                m_deadKeyState = 0;
                for (UniCharCount i = 0; i < count; ++i) {
                    ids.push_back(IOSXKeyResource::unicharToKeyID(chars[i]));
                }
                adjustAltGrModifier(ids, maskOut, isCommand);
                return mapVirtualKeyToKeyButton(vkCode);
            }
            return 0;
        }
    }

    return 0;
}

bool
OSXKeyState::fakeCtrlAltDel()
{
    // pass keys through unchanged
    return false;
}

bool
OSXKeyState::fakeMediaKey(KeyID id)
{
    return fakeNativeMediaKey(id);;
}

CGEventFlags
OSXKeyState::getModifierStateAsOSXFlags()
{
    CGEventFlags modifiers = CGEventFlags(0);

    if (m_shiftPressed) {
        modifiers |= CGEventFlags(kCGEventFlagMaskShift);
    }

    if (m_controlPressed) {
        modifiers |= CGEventFlags(kCGEventFlagMaskControl);
    }

    if (m_altPressed) {
        modifiers |= CGEventFlags(kCGEventFlagMaskAlternate);
    }

    if (m_superPressed) {
        modifiers |= CGEventFlags(kCGEventFlagMaskCommand);
    }

    if (m_capsPressed) {
        modifiers |= CGEventFlags(kCGEventFlagMaskAlphaShift);
    }

    return modifiers;
}

KeyModifierMask
OSXKeyState::pollActiveModifiers() const
{
    // falsely assumed that the mask returned by GetCurrentKeyModifiers()
    // was the same as a CGEventFlags (which is what mapModifiersFromOSX
    // expects). patch by Marc
    UInt32 mask = GetCurrentKeyModifiers();
    KeyModifierMask outMask = 0;

    if ((mask & shiftKey) != 0) {
        outMask |= KeyModifierShift;
    }
    if ((mask & controlKey) != 0) {
        outMask |= KeyModifierControl;
    }
    if ((mask & optionKey) != 0) {
        outMask |= KeyModifierAlt;
    }
    if ((mask & cmdKey) != 0) {
        outMask |= KeyModifierSuper;
    }
    if ((mask & alphaLock) != 0) {
        outMask |= KeyModifierCapsLock;
    }
    if ((mask & s_osxNumLock) != 0) {
        outMask |= KeyModifierNumLock;
    }

    LOG((CLOG_DEBUG1 "mask=%04x outMask=%04x", mask, outMask));
    return outMask;
}

SInt32
OSXKeyState::pollActiveGroup() const
{
    TISInputSourceRef keyboardLayout = TISCopyCurrentKeyboardLayoutInputSource();
    CFDataRef id = (CFDataRef)TISGetInputSourceProperty(
                        keyboardLayout, kTISPropertyInputSourceID);

    GroupMap::const_iterator i = m_groupMap.find(id);
    if (i != m_groupMap.end()) {
        return i->second;
    }

    LOG((CLOG_DEBUG "can't get the active group, use the first group instead"));

    return 0;
}

void
OSXKeyState::pollPressedKeys(KeyButtonSet& pressedKeys) const
{
    ::KeyMap km;
    GetKeys(km);
    const UInt8* m = reinterpret_cast<const UInt8*>(km);
    for (UInt32 i = 0; i < 16; ++i) {
        for (UInt32 j = 0; j < 8; ++j) {
            if ((m[i] & (1u << j)) != 0) {
                pressedKeys.insert(mapVirtualKeyToKeyButton(8 * i + j));
            }
        }
    }
}

void
OSXKeyState::getKeyMap(barrier::KeyMap& keyMap)
{
    // update keyboard groups
    if (getGroups(m_groups)) {
        m_groupMap.clear();
        SInt32 numGroups = (SInt32)m_groups.size();
        for (SInt32 g = 0; g < numGroups; ++g) {
            CFDataRef id = (CFDataRef)TISGetInputSourceProperty(
                                m_groups[g], kTISPropertyInputSourceID);
            m_groupMap[id] = g;
        }
    }

    UInt32 keyboardType = LMGetKbdType();
    for (SInt32 g = 0, n = (SInt32)m_groups.size(); g < n; ++g) {
        // add special keys
        getKeyMapForSpecialKeys(keyMap, g);

        const void* resource;
        bool layoutValid = false;

        // add regular keys
        // try uchr resource first
        CFDataRef resourceRef = (CFDataRef)TISGetInputSourceProperty(
            m_groups[g], kTISPropertyUnicodeKeyLayoutData);

        layoutValid = resourceRef != NULL;
        if (layoutValid)
            resource = CFDataGetBytePtr(resourceRef);

        if (layoutValid) {
            OSXUchrKeyResource uchr(resource, keyboardType);
            if (uchr.isValid()) {
                LOG((CLOG_DEBUG1 "using uchr resource for group %d", g));
                getKeyMap(keyMap, g, uchr);
                continue;
            }
        }

        LOG((CLOG_DEBUG1 "no keyboard resource for group %d", g));
    }
}

static io_connect_t getEventDriver(void)
{
    static mach_port_t sEventDrvrRef = 0;
    mach_port_t masterPort, service, iter;
    kern_return_t kr;

    if (!sEventDrvrRef) {
        // Get master device port
        kr = IOMasterPort(bootstrap_port, &masterPort);
        assert(KERN_SUCCESS == kr);

        kr = IOServiceGetMatchingServices(masterPort,
                IOServiceMatching(kIOHIDSystemClass), &iter);
        assert(KERN_SUCCESS == kr);

        service = IOIteratorNext(iter);
        assert(service);

        kr = IOServiceOpen(service, mach_task_self(),
                kIOHIDParamConnectType, &sEventDrvrRef);
        assert(KERN_SUCCESS == kr);

        IOObjectRelease(service);
        IOObjectRelease(iter);
    }

    return sEventDrvrRef;
}

void
OSXKeyState::postHIDVirtualKey(const UInt8 virtualKeyCode,
                const bool postDown)
{
    static UInt32 modifiers = 0;

    NXEventData event;
    IOGPoint loc = { 0, 0 };
    UInt32 modifiersDelta = 0;

    bzero(&event, sizeof(NXEventData));

    switch (virtualKeyCode)
    {
    case s_shiftVK:
    case s_superVK:
    case s_altVK:
    case s_controlVK:
    case s_capsLockVK:
        switch (virtualKeyCode)
        {
        case s_shiftVK:
                modifiersDelta = NX_SHIFTMASK | NX_DEVICELSHIFTKEYMASK;
                m_shiftPressed = postDown;
                break;
        case s_superVK:
                modifiersDelta = NX_COMMANDMASK | NX_DEVICELCMDKEYMASK;
                m_superPressed = postDown;
                break;
        case s_altVK:
                modifiersDelta = NX_ALTERNATEMASK | NX_DEVICELALTKEYMASK;
                m_altPressed = postDown;
                break;
        case s_controlVK:
                modifiersDelta = NX_CONTROLMASK | NX_DEVICELCTLKEYMASK;
                m_controlPressed = postDown;
                break;
        case s_capsLockVK:
                modifiersDelta = NX_ALPHASHIFTMASK;
                m_capsPressed = postDown;
                break;
        }

        // update the modifier bit
        if (postDown) {
            modifiers |= modifiersDelta;
        }
        else {
            modifiers &= ~modifiersDelta;
        }

        kern_return_t kr;
        event.key.keyCode = virtualKeyCode;
        kr = IOHIDPostEvent(getEventDriver(), NX_FLAGSCHANGED, loc,
                &event, kNXEventDataVersion, modifiers, true);
        assert(KERN_SUCCESS == kr);
        break;

    default:
        event.key.repeat = false;
        event.key.keyCode = virtualKeyCode;
        event.key.origCharSet = event.key.charSet = NX_ASCIISET;
        event.key.origCharCode = event.key.charCode = 0;
        kr = IOHIDPostEvent(getEventDriver(),
                postDown ? NX_KEYDOWN : NX_KEYUP,
                loc, &event, kNXEventDataVersion, 0, false);
        assert(KERN_SUCCESS == kr);
        break;
    }
}

void
OSXKeyState::fakeKey(const Keystroke& keystroke)
{
    switch (keystroke.m_type) {
    case Keystroke::kButton: {

        KeyButton button = keystroke.m_data.m_button.m_button;
        bool keyDown = keystroke.m_data.m_button.m_press;
        CGKeyCode virtualKey = mapKeyButtonToVirtualKey(button);

        LOG((CLOG_DEBUG1
            "  button=0x%04x virtualKey=0x%04x keyDown=%s",
            button, virtualKey, keyDown ? "down" : "up"));

        postHIDVirtualKey(virtualKey, keyDown);

        break;
    }

    case Keystroke::kGroup: {
        SInt32 group = keystroke.m_data.m_group.m_group;
        if (keystroke.m_data.m_group.m_absolute) {
            LOG((CLOG_DEBUG1 "  group %d", group));
            setGroup(group);
        }
        else {
            LOG((CLOG_DEBUG1 "  group %+d", group));
            setGroup(getEffectiveGroup(pollActiveGroup(), group));
        }
        break;
    }
    }
}

void
OSXKeyState::getKeyMapForSpecialKeys(barrier::KeyMap& keyMap, SInt32 group) const
{
    // special keys are insensitive to modifers and none are dead keys
    barrier::KeyMap::KeyItem item;
    for (size_t i = 0; i < sizeof(s_controlKeys) /
                                sizeof(s_controlKeys[0]); ++i) {
        const KeyEntry& entry = s_controlKeys[i];
        item.m_id        = entry.m_keyID;
        item.m_group     = group;
        item.m_button    = mapVirtualKeyToKeyButton(entry.m_virtualKey);
        item.m_required  = 0;
        item.m_sensitive = 0;
        item.m_dead      = false;
        item.m_client    = 0;
        barrier::KeyMap::initModifierKey(item);
        keyMap.addKeyEntry(item);

        if (item.m_lock) {
            // all locking keys are half duplex on OS X
            keyMap.addHalfDuplexButton(item.m_button);
        }
    }

    // note:  we don't special case the number pad keys.  querying the
    // mac keyboard returns the non-keypad version of those keys but
    // a KeyState always provides a mapping from keypad keys to
    // non-keypad keys so we'll be able to generate the characters
    // anyway.
}

bool
OSXKeyState::getKeyMap(barrier::KeyMap& keyMap,
                SInt32 group, const IOSXKeyResource& r) const
{
    if (!r.isValid()) {
        return false;
    }

    // space for all possible modifier combinations
    std::vector<bool> modifiers(r.getNumModifierCombinations());

    // make space for the keys that any single button can synthesize
    std::vector<std::pair<KeyID, bool> > buttonKeys(r.getNumTables());

    // iterate over each button
    barrier::KeyMap::KeyItem item;
    for (UInt32 i = 0; i < r.getNumButtons(); ++i) {
        item.m_button = mapVirtualKeyToKeyButton(i);

        // the KeyIDs we've already handled
        std::set<KeyID> keys;

        // convert the entry in each table for this button to a KeyID
        for (UInt32 j = 0; j < r.getNumTables(); ++j) {
            buttonKeys[j].first  = r.getKey(j, i);
            buttonKeys[j].second = barrier::KeyMap::isDeadKey(buttonKeys[j].first);
        }

        // iterate over each character table
        for (UInt32 j = 0; j < r.getNumTables(); ++j) {
            // get the KeyID for the button/table
            KeyID id = buttonKeys[j].first;
            if (id == kKeyNone) {
                continue;
            }

            // if we've already handled the KeyID in the table then
            // move on to the next table
            if (keys.count(id) > 0) {
                continue;
            }
            keys.insert(id);

            // prepare item.  the client state is 1 for dead keys.
            item.m_id     = id;
            item.m_group  = group;
            item.m_dead   = buttonKeys[j].second;
            item.m_client = buttonKeys[j].second ? 1 : 0;
            barrier::KeyMap::initModifierKey(item);
            if (item.m_lock) {
                // all locking keys are half duplex on OS X
                keyMap.addHalfDuplexButton(i);
            }

            // collect the tables that map to the same KeyID.  we know it
            // can't be any earlier tables because of the check above.
            std::set<UInt8> tables;
            tables.insert(static_cast<UInt8>(j));
            for (UInt32 k = j + 1; k < r.getNumTables(); ++k) {
                if (buttonKeys[k].first == id) {
                    tables.insert(static_cast<UInt8>(k));
                }
            }

            // collect the modifier combinations that map to any of the
            // tables we just collected
            for (UInt32 k = 0; k < r.getNumModifierCombinations(); ++k) {
                modifiers[k] = (tables.count(r.getTableForModifier(k)) > 0);
            }

            // figure out which modifiers the key is sensitive to.  the
            // key is insensitive to a modifier if for every modifier mask
            // with the modifier bit unset in the modifiers we also find
            // the same mask with the bit set.
            //
            // we ignore a few modifiers that we know aren't important
            // for generating characters.  in fact, we want to ignore any
            // characters generated by the control key.  we don't map
            // those and instead expect the control modifier plus a key.
            UInt32 sensitive = 0;
            for (UInt32 k = 0; (1u << k) <
                                r.getNumModifierCombinations(); ++k) {
                UInt32 bit = (1u << k);
                if ((bit << 8) == cmdKey ||
                    (bit << 8) == controlKey ||
                    (bit << 8) == rightControlKey) {
                    continue;
                }
                for (UInt32 m = 0; m < r.getNumModifierCombinations(); ++m) {
                    if (modifiers[m] != modifiers[m ^ bit]) {
                        sensitive |= bit;
                        break;
                    }
                }
            }

            // find each required modifier mask.  the key can be synthesized
            // using any of the masks.
            std::set<UInt32> required;
            for (UInt32 k = 0; k < r.getNumModifierCombinations(); ++k) {
                if ((k & sensitive) == k && modifiers[k & sensitive]) {
                    required.insert(k);
                }
            }

            // now add a key entry for each key/required modifier pair.
            item.m_sensitive = mapModifiersFromOSX(sensitive << 16);
            for (std::set<UInt32>::iterator k = required.begin();
                                            k != required.end(); ++k) {
                item.m_required = mapModifiersFromOSX(*k << 16);
                keyMap.addKeyEntry(item);
            }
        }
    }

    return true;
}

bool
OSXKeyState::mapBarrierHotKeyToMac(KeyID key, KeyModifierMask mask,
                UInt32 &macVirtualKey, UInt32 &macModifierMask) const
{
    // look up button for key
    KeyButton button = getButton(key, pollActiveGroup());
    if (button == 0 && key != kKeyNone) {
        return false;
    }
    macVirtualKey = mapKeyButtonToVirtualKey(button);

    // calculate modifier mask
    macModifierMask = 0;
    if ((mask & KeyModifierShift) != 0) {
        macModifierMask |= shiftKey;
    }
    if ((mask & KeyModifierControl) != 0) {
        macModifierMask |= controlKey;
    }
    if ((mask & KeyModifierAlt) != 0) {
        macModifierMask |= cmdKey;
    }
    if ((mask & KeyModifierSuper) != 0) {
        macModifierMask |= optionKey;
    }
    if ((mask & KeyModifierCapsLock) != 0) {
        macModifierMask |= alphaLock;
    }
    if ((mask & KeyModifierNumLock) != 0) {
        macModifierMask |= s_osxNumLock;
    }

    return true;
}

void
OSXKeyState::handleModifierKeys(void* target,
                KeyModifierMask oldMask, KeyModifierMask newMask)
{
    // compute changed modifiers
    KeyModifierMask changed = (oldMask ^ newMask);

    // synthesize changed modifier keys
    if ((changed & KeyModifierShift) != 0) {
        handleModifierKey(target, s_shiftVK, kKeyShift_L,
                            (newMask & KeyModifierShift) != 0, newMask);
    }
    if ((changed & KeyModifierControl) != 0) {
        handleModifierKey(target, s_controlVK, kKeyControl_L,
                            (newMask & KeyModifierControl) != 0, newMask);
    }
    if ((changed & KeyModifierAlt) != 0) {
        handleModifierKey(target, s_altVK, kKeyAlt_L,
                            (newMask & KeyModifierAlt) != 0, newMask);
    }
    if ((changed & KeyModifierSuper) != 0) {
        handleModifierKey(target, s_superVK, kKeySuper_L,
                            (newMask & KeyModifierSuper) != 0, newMask);
    }
    if ((changed & KeyModifierCapsLock) != 0) {
        handleModifierKey(target, s_capsLockVK, kKeyCapsLock,
                            (newMask & KeyModifierCapsLock) != 0, newMask);
    }
    if ((changed & KeyModifierNumLock) != 0) {
        handleModifierKey(target, s_numLockVK, kKeyNumLock,
                            (newMask & KeyModifierNumLock) != 0, newMask);
    }
}

void
OSXKeyState::handleModifierKey(void* target,
                UInt32 virtualKey, KeyID id,
                bool down, KeyModifierMask newMask)
{
    KeyButton button = mapVirtualKeyToKeyButton(virtualKey);
    onKey(button, down, newMask);
    sendKeyEvent(target, down, false, id, newMask, 0, button);
}

bool
OSXKeyState::getGroups(GroupList& groups) const
{
    CFIndex n;
    bool gotLayouts = false;

    // get number of layouts
    CFStringRef keys[] = { kTISPropertyInputSourceCategory };
    CFStringRef values[] = { kTISCategoryKeyboardInputSource };
    CFDictionaryRef dict = CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, 1, NULL, NULL);
    CFArrayRef kbds = TISCreateInputSourceList(dict, false);
    n = CFArrayGetCount(kbds);
    gotLayouts = (n != 0);

    if (!gotLayouts) {
        LOG((CLOG_DEBUG1 "can't get keyboard layouts"));
        return false;
    }

    // get each layout
    groups.clear();
    for (CFIndex i = 0; i < n; ++i) {
        bool addToGroups = true;
        TISInputSourceRef keyboardLayout =
            (TISInputSourceRef)CFArrayGetValueAtIndex(kbds, i);

        if (addToGroups)
            groups.push_back(keyboardLayout);
    }
    return true;
}

void
OSXKeyState::setGroup(SInt32 group)
{
    TISSetInputMethodKeyboardLayoutOverride(m_groups[group]);
}

void
OSXKeyState::checkKeyboardLayout()
{
    // XXX -- should call this when notified that groups have changed.
    // if no notification for that then we should poll.
    GroupList groups;
    if (getGroups(groups) && groups != m_groups) {
        updateKeyMap();
        updateKeyState();
    }
}

void
OSXKeyState::adjustAltGrModifier(const KeyIDs& ids,
                KeyModifierMask* mask, bool isCommand) const
{
    if (!isCommand) {
        for (KeyIDs::const_iterator i = ids.begin(); i != ids.end(); ++i) {
            KeyID id = *i;
            if (id != kKeyNone &&
                ((id < 0xe000u || id > 0xefffu) ||
                (id >= kKeyKP_Equal && id <= kKeyKP_9))) {
                *mask |= KeyModifierAltGr;
                return;
            }
        }
    }
}

KeyButton
OSXKeyState::mapVirtualKeyToKeyButton(UInt32 keyCode)
{
    // 'A' maps to 0 so shift every id
    return static_cast<KeyButton>(keyCode + KeyButtonOffset);
}

UInt32
OSXKeyState::mapKeyButtonToVirtualKey(KeyButton keyButton)
{
    return static_cast<UInt32>(keyButton - KeyButtonOffset);
}
