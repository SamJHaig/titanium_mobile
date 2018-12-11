/*
 * Appcelerator Titanium Mobile
 * Copyright (c) 2011-Present by Appcelerator, Inc. All Rights Reserved.
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 */
/* eslint-env mocha */
/* global Ti */
/* eslint no-unused-expressions: "off" */
'use strict';
var should = require('./utilities/assertions');

describe('Titanium.UI.Window', function () {
	var win,
		rootWindow;

	this.timeout(5000);

	// Create and open a root window for the rest of the below child window tests to use as a parent.
	// We're not going to close this window until the end of this test suite.
	// Note: Android needs this so that closing the last window won't back us out of the app.
	before(function (finish) {
		rootWindow = Ti.UI.createWindow();
		rootWindow.addEventListener('open', function () {
			finish();
		});
		rootWindow.open();
	});

	after(function (finish) {
		rootWindow.addEventListener('close', function () {
			finish();
		});
		rootWindow.close();
	});

	afterEach(function (done) {
		if (win) {
			win.close();
		}
		win = null;

		// timeout to allow window to close
		setTimeout(() => {
			done();
		}, 500);
	});

	it.ios('.safeAreaPadding for window inside navigation window with extendSafeArea true', function (finish) {
		var window = Ti.UI.createWindow({
			extendSafeArea: true,
		});
		win = Ti.UI.createNavigationWindow({
			window: window
		});
		window.addEventListener('postlayout', function () {
			try {
				var padding = window.safeAreaPadding;
				should(padding).be.a.Object;
				should(padding.left).be.eql(0);
				should(padding.top).be.eql(0);
				should(padding.right).be.eql(0);
				should(padding.bottom).be.eql(0);
				finish();
			} catch (err) {
				finish(err);
			}
		});
		win.open();
	});
});