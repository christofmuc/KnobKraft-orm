# KnobKraft Orm Adaptation Testing Guide

Now, after you have written your own adaptation, you might wonder how to test it apart from doing it manually. Actually, 
my recommendation is even to write the tests while you are developing the adaptation, nearly in a test-first manner, 
because it can make you much more confident that the code you have written already works.

Let us distinguish between different things that need testing, and then device a testing strategy:

1. We need to test the functions we have implemented, and make sure they do what we expected. This would be similar to a unit test in traditional program testing.
2. The adaptation needs to be fed with actual data from the synth, to see that it can parse and convert that data appropriately.
3. The communication with a real device needs to be tested, to make sure the device actually behaves like the documentation suggests (if we have any documentation at all)

Luckily for us, the test items 1 and 2 can be fairly standardized for all adaptations because they all implement the same
functions and capabilities. Given test data matching the synth, they should even pass the same tests!

To allow for this, a couple of standard tests are provided in the file [test_adaptations.py](test_adaptations.py) does exactly that.
The way this works is that you need to implement a single function in your adaptation, and it will be picked up and tested by our test suite.

Details follow below.

## Creating a Python virtual env

Best practice when working with Python is to always use virtual environments of Python for each the various
projects that might end up on your computer. A virtual environment is like a local, shielded installation of 
Python in a specific version, into which packages can be installed and uninstalled without modifying the global
Python installation.

To create a virtual environment to run tests in, use a command line and type

    python -m venv venvdir

This will create a virtual environment in the subdirectory venvdir. We can activate it on Windows with

    venvdir\Scripts\activate

on Linux you'll do

    source venvdir/bin/activate.sh

With the virtual env activated, we can use the pip package manager to install the required packages for running the tests. 
Just do

    pip install -r .\adaptions\requirements.txt

which will install the pytest package and other prerequisites.


## Running the tests

All tests are implemented as pytest tests, so the correct way to run them all is use the standard method for pytests, just 
type in your prepared virtual environment, make sure to cd into the adaptions subdirectory:

     cd adaptions
     python -m pytest . -all

This should be run in the source's root directory.

Note this will run all tests in the adaptation directory, so any file starting with ´test_´ will be considered a pytest and run.

Run all generic tests skipping those synth specific tests, just do 

    python -m pytest test_adaptations.py  --all  

If you want to run only the tests for a single adaptation - most likely the one you are currently working on - this is the 
way to select one adaptation:

    python -m pytest test_adaptations.py --adaptation Matrix1000.py

will run only the generic tests on the Matrix1000 adaptation. 

## Implementing your own tests

### Feeding the standard test suite with synth specific data

As mentioned above, there are a lot of tests already implemented that your adaptation must pass. But to acticate them, 
you need to implement one more method in your adaptation module:

    import testing

    def make_test_data():

The presence of make_test_data enables the test suite found in test_adaptations.py for this adaptation. For this to work, 
the make_test_data function needs to return a specific Data Structure.

Let's have a look at a real life example and explore what is in there:

    def make_test_data():
        def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
            yield testing.ProgramTestData(message=data.all_messages[5], number=261)  # It is bank 3, so it starts at 256 + 5 = 261
    
        return testing.TestData(sysex="testData/Evolver_bank3_1-0.syx", program_generator=programs)

The make_test_data() method returns an instance of the testing.TestData class. This class has a lot of parameters, 
not all of them are required. Have a look at the implementation in the (testing/test_data.py) file.

What we do here for the Evolver tests is to specify a sysex file, which is specific to the Evolver and which contains a full bank dump,
and we specify a program_generator function. That function is right above the return statement, and gets the testing.TestData structure
passed as an argument. It is a Python generator function, meaning it can generate one or more return values using the yield 
keyword from Python instead of just returning once with return. 

In this case however it just rather arbitrarily picks up the message with the index 5 from the data.all_messages structure, which
contains the content of the sysex file we specified when constructing the TestData class below, and additionally specifies that
this program has to have internal program number 261.

Depending on how much data we produce in the make_test_data() function and which methods and capabilities our adaptation
has implemented, the generic test suite selects which tests are applicable and can be run.

I recommend using PyCharm as an IDE, and setting up the test run as pytest run configurations. This additionally allows you 
to easily use the Coverage tool in PyCharm and measure which lines of your adaptation are already tested by the tests and which not,
giving you good information on which tests to add.

Here is a more complex and more complete picture of a make_test_data() implementation:

    def make_test_data():
        def programs(test_data: testing.TestData) -> List[testing.ProgramTestData]:
            yield testing.ProgramTestData(message=test_data.all_messages[0], name="Welcome Back Tom!", number=0)
            yield testing.ProgramTestData(message=test_data.all_messages[200], name="I Dream of Phasing", number=200)
    
    return testing.TestData(sysex="testData/Sequential_OB6/OB6_Programs_v1.01.syx", program_generator=programs)

This is similar to the Evolver example, but shows how to return more than one value from the program generator, and 
that you can additionally specify the expected program name enabling testing of the nameForDump() and rename() functions.

To write more extensive test data, have a look at the implementation of the tests in test_adaptations.py and the 
test data data structures in testing/test_data.py. You can get as complex as this and beyond:

    def make_test_data():
    
        def make_patches(test_data: testing.TestData) -> List[testing.ProgramTestData]:
            names_of_first_program = ["Talkbox001", "OrganX01..", "unknown"]
            count = 0
            for message in test_data.all_messages:
                assert isPartOfBankDump(message)
                patch_data = extractPatchesFromBank(message)
                if patch_data is not None:
                    patches = knobkraft.sysex.splitSysex(patch_data)
                    yield testing.ProgramTestData(message=patches[0], name=names_of_first_program[count])
                    count += 1
    
        return testing.TestData(sysex=R"testData/yamahaDX7II-STUDIOREINE BANK.syx", edit_buffer_generator=make_patches)

This is the same pattern as before, but you can see that more Yamaha-adaptation specific functions are use to prepare the 
test data and yield exactly the right message. Each return value of the program generator, or in this case the edit_buffer_generator,
which works the same way but is used to produce data to test the edit buffer capability instead of the program dump capability,
is an instance of the ProgramTestData class also implemented in test_data.py.

Here is an example (slightly shortened, find the full version in Roland_XV3080.py) that shows that you can also provide
example device detect messages and expected answers to test your device detection code as well:

    def make_test_data():
        def programs(data: testing.TestData) -> List[testing.ProgramTestData]:
        patch = []
        names = ["RedPowerBass", "Sinus QSB", "Super W Bass"]
        i = 0
        # Extract the first 3 programs from the sysex dump loaded, and yield them with name and number to the test code
        for message in data.all_messages:
            if xv_3080.isPartOfSingleProgramDump(message):
            <snip>

        return testing.TestData(sysex="testData/jv1080_AGSOUND1.SYX",
                                program_generator=programs,
                                program_dump_request="f0 41 10 00 10 11 30 00 00 00 00 00 00 4f 01 f7",
                                device_detect_call="f0 7e 00 06 01 f7",
                                device_detect_reply=("f0 7e 10 06 02 41 10 01 00 00 00 00 00 00 f7", 0))


That's it, you don't need to write more code to get your unit tests going, unless you want to make tests that are
very specific. For this, see the next section.

### Synth specific tests

As an example, let's assume you want to implement an adaptation for the Groove Synthesis 3rd Wave synth. Some of its
functions might be so specific,  that the generic tests won't cover that functionality. Typically, this could be special 
sysex byte packing algorithms or intricate bank dump conversions.

Just create an appropriate file starting with test_ and it will be picked up by the pytest command above, making sure your new
tests will be run every time the tests run.

We would create a file test_ThirdWave.py and for example implement the following function:

    import ThirdWave

    def test_name():
        adaptation_name = ThirdWave.name()
        assert adaptation_name == "3rd Wave" 

## Testing the communication with the Synth

If you have written an adaptation and provide test data using the make_test_data() method, and it passes the tests,
the most exciting part of testing comes - testing with the real device.

I do this in the KnobKraft Orm itself - first making sure that it can load my adaptation and offer it in the setup tab, I 
turn the new synth on (and usually all others off), create a new database to not screw up my production database with all my 
archived patches, and give it a go. 

The most important tools for this phase are the log window in the Orm (you can print into it from your Python adaptation using
the regular print() function) and the MIDI log, which shows you both incoming and outgoing MIDI messages.

That is usually enough for me to get enough information to fix any bugs that might hinder communication of the 
Orm with the synth.

Let me know if this helps you and don't hesitate to contact me with any questions left!
