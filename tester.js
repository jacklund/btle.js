var util = require('util');

// Get all functions in module.exports that start with "test"
function getTests() {
  var tests = [];
  var patt = /^test/;
  for (var k in require.main.exports)
    if (patt.test(k)) tests.push({name: k, func: require.main.exports[k]});

  return tests;
}

// Timeout value
var timeout = 15000;
module.exports.setTimeout = function(time) {
  timeout = time;
}

// Main test runner
module.exports.runTests = function() {
  // Grab the arguments and turn them into an array
  var args = [];
  for (var i = 0; i < arguments.length; i++)
    args.push(arguments[i]);

  // Get all the test functions
  var tests = getTests();
  var currentTest = null;

  // Run the next test
  module.exports.nextTest = nextTest = function() {
    // If we just finished running a test, check for success
    if (currentTest) {
      if (currentTest.success) {
        console.log("%s succeeded!", currentTest.name);
      } else {
        console.log("%s failed!", currentTest.name);
      }
    }

    // Get the next test
    currentTest = tests.shift();
    if (currentTest) currentTest.success = false;

    // Create the callback wrapper function
    module.exports.createCallback = function(f) {
      // Set the timer for the callback
      var id = setTimeout(function() {
        console.log("Timeout getting callback for %s", currentTest.name);
        currentTest.success = false;
        nextTest();
      }, timeout);

      // Return the callback wrapper function
      return function() {
        // Turn off the timer
        clearTimeout(id);

        // Gather the arguments
        var args = [];
        for (var i = 0; i < arguments.length; i++)
          args.push(arguments[i]);

        // Call the wrapped callback
        try {
          f.apply(this, args);
          currentTest.success = true;
        } catch (err) {
          console.log("Error running %s: %s", currentTest.name, util.inspect(err));
          currentTest.success = false;
          nextTest();
        }
      }
    }

    // If we have a test to run, run it
    if (currentTest) {
      try {
        console.log(currentTest.name);
        currentTest.func.apply(this, args);
        currentTest.success = true;
      } catch (err) {
        console.log("Error running %s: %s", currentTest.name, util.inspect(err));
        currentTest.success = false;
        nextTest();
      }
    } else {
      console.log("Done!");
      process.exit(0);
    }
  }

  // Start the tests
  nextTest();
}
