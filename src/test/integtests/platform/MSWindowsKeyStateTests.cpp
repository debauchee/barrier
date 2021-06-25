/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2011 Nick Bolton
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

#define BARRIER_TEST_ENV

#include "test/mock/barrier/MockEventQueue.h"
#include "test/mock/barrier/MockKeyMap.h"
#include "platform/MSWindowsKeyState.h"
#include "platform/MSWindowsDesks.h"
#include "platform/MSWindowsScreen.h"
#include "platform/MSWindowsScreenSaver.h"
#include "base/TMethodJob.h"

#include "test/global/gtest.h"
#include "test/global/gmock.h"

// wParam = flags, HIBYTE(lParam) = virtual key, LOBYTE(lParam) = scan code
#define BARRIER_MSG_FAKE_KEY		BARRIER_HOOK_LAST_MSG + 4

using ::testing::_;
using ::testing::NiceMock;

class MSWindowsKeyStateTests : public ::testing::Test
{
protected:
	virtual void SetUp()
	{
		m_screensaver = new MSWindowsScreenSaver();
	}

	virtual void TearDown()
	{
		delete m_screensaver;
	}

	MSWindowsDesks* newDesks(IEventQueue* eventQueue)
	{
		return new MSWindowsDesks(
			true, false, m_screensaver, eventQueue,
			new TMethodJob<MSWindowsKeyStateTests>(
				this, &MSWindowsKeyStateTests::updateKeysCB), false, false);
	}

	void* getEventTarget() const
	{
		return const_cast<MSWindowsKeyStateTests*>(this);
	}

private:
	void updateKeysCB(void*) { }
	IScreenSaver* m_screensaver;
	MSWindowsHook m_hook;
};

TEST_F(MSWindowsKeyStateTests, disable_eventQueueNotUsed)
{
	NiceMock<MockEventQueue> eventQueue;
	MSWindowsDesks* desks = newDesks(&eventQueue);
	MockKeyMap keyMap;
	MSWindowsKeyState keyState(desks, getEventTarget(), &eventQueue, keyMap);

	EXPECT_CALL(eventQueue, removeHandler(_, _)).Times(0);

	keyState.disable();
	delete desks;
}

TEST_F(MSWindowsKeyStateTests, testAutoRepeat_noRepeatAndButtonIsZero_resultIsTrue)
{
	NiceMock<MockEventQueue> eventQueue;
	MSWindowsDesks* desks = newDesks(&eventQueue);
	MockKeyMap keyMap;
	MSWindowsKeyState keyState(desks, getEventTarget(), &eventQueue, keyMap);
	keyState.setLastDown(1);

	bool actual = keyState.testAutoRepeat(true, false, 1);

	ASSERT_TRUE(actual);
	delete desks;
}

TEST_F(MSWindowsKeyStateTests, testAutoRepeat_pressFalse_lastDownIsZero)
{
	NiceMock<MockEventQueue> eventQueue;
	MSWindowsDesks* desks = newDesks(&eventQueue);
	MockKeyMap keyMap;
	MSWindowsKeyState keyState(desks, getEventTarget(), &eventQueue, keyMap);
	keyState.setLastDown(1);

	keyState.testAutoRepeat(false, false, 1);

	ASSERT_EQ(0, keyState.getLastDown());
	delete desks;
}

TEST_F(MSWindowsKeyStateTests, saveModifiers_noModifiers_savedModifiers0)
{
	NiceMock<MockEventQueue> eventQueue;
	MSWindowsDesks* desks = newDesks(&eventQueue);
	MockKeyMap keyMap;
	MSWindowsKeyState keyState(desks, getEventTarget(), &eventQueue, keyMap);

	keyState.saveModifiers();

	ASSERT_EQ(0, keyState.getSavedModifiers());
	delete desks;
}

TEST_F(MSWindowsKeyStateTests, testKoreanLocale_inputModeKey_resultCorrectKeyID)
{
	NiceMock<MockEventQueue> eventQueue;
	MSWindowsDesks* desks = newDesks(&eventQueue);
	MockKeyMap keyMap;
	MSWindowsKeyState keyState(desks, getEventTarget(), &eventQueue, keyMap);

	keyState.setKeyLayout((HKL)0x00000412u);	// for ko-KR local ID
	ASSERT_EQ(0xEF31, keyState.getKeyID(0x15u, 0x1f2u));	// VK_HANGUL from Hangul key
	ASSERT_EQ(0xEF34, keyState.getKeyID(0x19u, 0x1f1u));	// VK_HANJA from Hanja key
	ASSERT_EQ(0xEF31, keyState.getKeyID(0x15u, 0x11du));	// VK_HANGUL from R-Alt key
	ASSERT_EQ(0xEF34, keyState.getKeyID(0x19u, 0x138u));	// VK_HANJA from R-Ctrl key

	keyState.setKeyLayout((HKL)0x00000411); // for ja-jp locale ID
	ASSERT_EQ(0xEF26, keyState.getKeyID(0x15u, 0x1du));	// VK_KANA
	ASSERT_EQ(0xEF2A, keyState.getKeyID(0x19u, 0x38u));	// VK_KANJI

	delete desks;
}

