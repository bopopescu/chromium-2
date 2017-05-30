// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.accounts.Account;
import android.app.Activity;
import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.multidex.ShadowMultiDex;
import org.robolectric.util.ActivityController;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests FirstRunFlowSequencer which contains the core logic of what should be shown during the
 * first run.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, application = BaseChromiumApplication.class,
        shadows = {ShadowMultiDex.class})
public class FirstRunFlowSequencerTest {
    /** Information for Google OS account */
    private static final String GOOGLE_ACCOUNT_TYPE = "com.google";
    private static final String DEFAULT_ACCOUNT = "test@gmail.com";

    /**
     * Testing version of FirstRunFlowSequencer that allows us to override all needed checks.
     */
    public static class TestFirstRunFlowSequencer extends FirstRunFlowSequencer {
        public Bundle returnedBundle;
        public boolean calledOnFlowIsKnown;
        public boolean calledSetDefaultMetricsAndCrashReporting;
        public boolean calledSetFirstRunFlowSignInComplete;

        public boolean isFirstRunFlowComplete;
        public boolean isSignedIn;
        public boolean isSyncAllowed;
        public Account[] googleAccounts;
        public boolean hasAnyUserSeenToS;
        public boolean shouldSkipFirstUseHints;
        public boolean isFirstRunEulaAccepted;
        public boolean shouldShowDataReductionPage;

        public TestFirstRunFlowSequencer(Activity activity, Bundle launcherProvidedProperties) {
            super(activity, launcherProvidedProperties);
        }

        @Override
        public void onFlowIsKnown(Bundle freProperties) {
            calledOnFlowIsKnown = true;
            if (freProperties != null) onNativeInitialized(freProperties);
            returnedBundle = freProperties;
        }

        @Override
        public boolean isFirstRunFlowComplete() {
            return isFirstRunFlowComplete;
        }

        @Override
        public boolean isSignedIn() {
            return isSignedIn;
        }

        @Override
        public boolean isSyncAllowed() {
            return isSyncAllowed;
        }

        @Override
        public Account[] getGoogleAccounts() {
            return googleAccounts;
        }

        @Override
        public boolean hasAnyUserSeenToS() {
            return hasAnyUserSeenToS;
        }

        @Override
        public boolean shouldSkipFirstUseHints() {
            return shouldSkipFirstUseHints;
        }

        @Override
        public boolean isFirstRunEulaAccepted() {
            return isFirstRunEulaAccepted;
        }

        @Override
        public boolean shouldShowDataReductionPage() {
            return shouldShowDataReductionPage;
        }

        @Override
        public void setDefaultMetricsAndCrashReporting() {
            calledSetDefaultMetricsAndCrashReporting = true;
        }

        @Override
        protected void setFirstRunFlowSignInComplete() {
            calledSetFirstRunFlowSignInComplete = true;
        }
    }

    ActivityController<Activity> mActivityController;
    TestFirstRunFlowSequencer mSequencer;

    @Before
    public void setUp() throws Exception {
        mActivityController = Robolectric.buildActivity(Activity.class);
        Bundle launchProperties = new Bundle();
        launchProperties.putBoolean(FirstRunActivity.EXTRA_USE_FRE_FLOW_SEQUENCER, true);
        mSequencer =
                new TestFirstRunFlowSequencer(mActivityController.setup().get(), launchProperties);
    }

    @After
    public void tearDown() {
        mActivityController.pause().stop().destroy();
    }

    @Test
    @Feature({"FirstRun"})
    public void testFirstRunComplete() {
        Account[] accounts = new Account[1];
        accounts[0] = new Account(DEFAULT_ACCOUNT, GOOGLE_ACCOUNT_TYPE);
        mSequencer.isFirstRunFlowComplete = true;
        mSequencer.isSignedIn = false;
        mSequencer.isSyncAllowed = true;
        mSequencer.googleAccounts = accounts;
        mSequencer.hasAnyUserSeenToS = true;
        mSequencer.shouldSkipFirstUseHints = false;
        mSequencer.isFirstRunEulaAccepted = true;
        mSequencer.initializeSharedState(false, // androidEduDevice
                false); // hasChildAccount
        mSequencer.processFreEnvironmentPreNative();
        assertTrue(mSequencer.calledOnFlowIsKnown);
        assertNull(mSequencer.returnedBundle);
        assertFalse(mSequencer.calledSetDefaultMetricsAndCrashReporting);
    }

    @Test
    @Feature({"FirstRun"})
    public void testStandardFlowTosNotSeen() {
        mSequencer.isFirstRunFlowComplete = false;
        mSequencer.isSignedIn = false;
        mSequencer.isSyncAllowed = true;
        mSequencer.googleAccounts = new Account[0];
        mSequencer.hasAnyUserSeenToS = false;
        mSequencer.shouldSkipFirstUseHints = false;
        mSequencer.shouldShowDataReductionPage = false;
        mSequencer.initializeSharedState(false, // androidEduDevice
                false); // hasChildAccount
        mSequencer.processFreEnvironmentPreNative();
        assertTrue(mSequencer.calledOnFlowIsKnown);
        assertTrue(mSequencer.returnedBundle.getBoolean(FirstRunActivity.SHOW_WELCOME_PAGE));
        assertTrue(mSequencer.returnedBundle.getBoolean(FirstRunActivity.SHOW_SIGNIN_PAGE));
        assertFalse(mSequencer.returnedBundle.getBoolean(
                FirstRunActivity.SHOW_DATA_REDUCTION_PAGE));
        assertFalse(mSequencer.returnedBundle.getBoolean(AccountFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, mSequencer.returnedBundle.size());
        assertTrue(mSequencer.calledSetDefaultMetricsAndCrashReporting);
        assertFalse(mSequencer.calledSetFirstRunFlowSignInComplete);
    }

    @Test
    @Feature({"FirstRun"})
    public void testStandardFlowTosSeenOneAccount() {
        Account[] accounts = new Account[1];
        accounts[0] = new Account(DEFAULT_ACCOUNT, GOOGLE_ACCOUNT_TYPE);
        mSequencer.isFirstRunFlowComplete = false;
        mSequencer.isSignedIn = false;
        mSequencer.isSyncAllowed = true;
        mSequencer.googleAccounts = accounts;
        mSequencer.hasAnyUserSeenToS = true;
        mSequencer.shouldSkipFirstUseHints = false;
        mSequencer.shouldShowDataReductionPage = false;
        mSequencer.initializeSharedState(false, // androidEduDevice
                false); // hasChildAccount
        mSequencer.processFreEnvironmentPreNative();
        assertTrue(mSequencer.calledOnFlowIsKnown);
        assertTrue(mSequencer.returnedBundle.getBoolean(FirstRunActivity.SHOW_WELCOME_PAGE));
        assertTrue(mSequencer.returnedBundle.getBoolean(FirstRunActivity.SHOW_SIGNIN_PAGE));
        assertFalse(mSequencer.returnedBundle.getBoolean(
                FirstRunActivity.SHOW_DATA_REDUCTION_PAGE));
        assertFalse(mSequencer.returnedBundle.getBoolean(AccountFirstRunFragment.IS_CHILD_ACCOUNT));
        assertTrue(mSequencer.returnedBundle.getBoolean(
                AccountFirstRunFragment.PRESELECT_BUT_ALLOW_TO_CHANGE));
        assertEquals(DEFAULT_ACCOUNT, mSequencer.returnedBundle.getString(
                AccountFirstRunFragment.FORCE_SIGNIN_ACCOUNT_TO));
        assertEquals(6, mSequencer.returnedBundle.size());
        assertTrue(mSequencer.calledSetDefaultMetricsAndCrashReporting);
        assertFalse(mSequencer.calledSetFirstRunFlowSignInComplete);
    }

    @Test
    @Feature({"FirstRun"})
    public void testStandardFlowOneChildAccount() {
        Account[] accounts = new Account[1];
        accounts[0] = new Account(DEFAULT_ACCOUNT, GOOGLE_ACCOUNT_TYPE);
        mSequencer.isFirstRunFlowComplete = false;
        mSequencer.isSignedIn = false;
        mSequencer.isSyncAllowed = true;
        mSequencer.googleAccounts = accounts;
        mSequencer.hasAnyUserSeenToS = false;
        mSequencer.shouldSkipFirstUseHints = false;
        mSequencer.shouldShowDataReductionPage = false;
        mSequencer.initializeSharedState(false, // androidEduDevice
                true); // hasChildAccount
        mSequencer.processFreEnvironmentPreNative();
        assertTrue(mSequencer.calledOnFlowIsKnown);
        assertTrue(mSequencer.returnedBundle.getBoolean(FirstRunActivity.SHOW_WELCOME_PAGE));
        assertTrue(mSequencer.returnedBundle.getBoolean(FirstRunActivity.SHOW_SIGNIN_PAGE));
        assertFalse(mSequencer.returnedBundle.getBoolean(
                FirstRunActivity.SHOW_DATA_REDUCTION_PAGE));
        assertTrue(mSequencer.returnedBundle.getBoolean(AccountFirstRunFragment.IS_CHILD_ACCOUNT));
        assertFalse(mSequencer.returnedBundle.getBoolean(
                AccountFirstRunFragment.PRESELECT_BUT_ALLOW_TO_CHANGE));
        assertEquals(DEFAULT_ACCOUNT, mSequencer.returnedBundle.getString(
                AccountFirstRunFragment.FORCE_SIGNIN_ACCOUNT_TO));
        assertEquals(6, mSequencer.returnedBundle.size());
        assertTrue(mSequencer.calledSetDefaultMetricsAndCrashReporting);
        assertTrue(mSequencer.calledSetFirstRunFlowSignInComplete);
    }

    @Test
    @Feature({"FirstRun"})
    public void testStandardFlowShowDataReductionPage() {
        mSequencer.isFirstRunFlowComplete = false;
        mSequencer.isSignedIn = false;
        mSequencer.isSyncAllowed = true;
        mSequencer.googleAccounts = new Account[0];
        mSequencer.hasAnyUserSeenToS = false;
        mSequencer.shouldSkipFirstUseHints = false;
        mSequencer.shouldShowDataReductionPage = true;
        mSequencer.initializeSharedState(false, // androidEduDevice
                false); // hasChildAccount
        mSequencer.processFreEnvironmentPreNative();
        assertTrue(mSequencer.calledOnFlowIsKnown);
        assertTrue(mSequencer.returnedBundle.getBoolean(FirstRunActivity.SHOW_WELCOME_PAGE));
        assertTrue(mSequencer.returnedBundle.getBoolean(FirstRunActivity.SHOW_SIGNIN_PAGE));
        assertTrue(mSequencer.returnedBundle.getBoolean(FirstRunActivity.SHOW_DATA_REDUCTION_PAGE));
        assertFalse(mSequencer.returnedBundle.getBoolean(AccountFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, mSequencer.returnedBundle.size());
        assertTrue(mSequencer.calledSetDefaultMetricsAndCrashReporting);
        assertFalse(mSequencer.calledSetFirstRunFlowSignInComplete);
    }
}