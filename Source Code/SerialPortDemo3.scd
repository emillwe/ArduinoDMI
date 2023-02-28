/*
Emmett Miller
Justin Schwiesow
CSE 474 Lab 4
Demo 3:
Gyroscope Data
08/17/2022

Note: Buffer scrub process and control bus based on a tutorial by
Dr. Marcin Pączkowski, UW DXARTS
https://github.com/dyfer/arduino-wifi-supercollider-sensors

Serial read based on a method by Eli Fieldsteel, University
of Illinois
https://www.youtube.com/watch?v=_NpivsEva5o
*/

// Don't print each OSC event
OSCFunc.trace(false);

// check usbmodem number from which to read
SerialPort.listDevices


// SETUP
(
// set up Net Address
~local = NetAddr.localAddr;

// initialize serial port
// NOTE: usbmodem number can change! Check with SerialPort.listDevices above
// NOTE: match baud rate to arduino sketch
~port = SerialPort.new("/dev/tty.usbmodem1101", 115200);
)



(
// boot server if not running
// control bus and output proxy
s.waitForBoot({
	~controlBus = Bus.control(s, 3);
	~proxy = ProxySpace(s);
});

 // Receive OSC: read values into control bus
~receiver = OSCdef(
	key: \gyrUpdate,
	func: {|msg| ~controlBus.setnSynchronous(msg[1..]);},
	path: '\readGyr',
	srcID: ~local
);
)

// output proxy - get signal from control bus
~proxy[\gyr] = {In.kr(~controlBus.index, ~controlBus.numChannels)};

// monitor accelerometer values
~proxy[\gyr].scope;



( // Start reading from serial bus
~charArray = [ ]; // char array read from serial port
~getValue = Routine.new({
	var ascii; // char input (single byte) from serial stream
	var neg; // multiplier for negative inputs
	var valueScaler = 2048.0; // normalize value to factor of g = 9.8 m/s^2
	var startOfVal = true; // flag to check for negative sign
	var max = 0; // current max value
	var accMax = 15.75; // maximum accelerometer after g-normalization

	// task loop
	{
		// read next byte into character (blocking)
		ascii = ~port.read.asAscii;

		// check for negative sign
		if(startOfVal, {
			if(ascii == $-, {neg = -1; startOfVal = false;}, {neg = 1; startOfVal = false;});
		});

		// add any decimal digit characters to char array
		if(ascii.isDecDigit, {~charArray = ~charArray.add(ascii);});

		// parse gyroscope data
		// an x, y, or z in serial input indicates end of value
		if(ascii == $x, {
			// get int value from char array
			~valx = ~charArray.collect(_.digit).convertDigits;
			~valx = ~valx * neg;
			~valx = ~valx / valueScaler; // normalize in terms of g = 9.8 m/s^2
			~valx = ~valx / accMax; // normalize to [-1.0, 1.0]

			// empty array
			~charArray = [ ];

			// check for negatives at next value
			startOfVal = true;
		});
		if(ascii == $y, {
			// get value from char array
			~valy = ~charArray.collect(_.digit).convertDigits;
			~valy - ~valy * neg;
			~valy = ~valy / valueScaler; // normalize in terms of g = 9.8 m/s^2
			~valy = ~valy / accMax; // normalize to [-1.0, 1.0]

			// empty array
			~charArray = [ ];

			// check for negatives at next value
			startOfVal = true;
		});
		if(ascii == $z, {
			// get value from char array
			~valz = ~charArray.collect(_.digit).convertDigits;
			~valz - ~valz * neg;
			~valz = ~valz / valueScaler; // normalize in terms of g = 9.8 m/s^2
			~valz = ~valz / accMax; // normalize to [-1.0, 1.0]

			// empty array
			~charArray = [ ];

			// check for negatives at next value
			startOfVal = true;
		});

		// send OSC message:
		~local.sendMsg('\readGyr', ~valx, ~valy, ~valz);
	}.loop;

}).play;
)

// stop reading from serial
~getValue.stop

// select a sound file
~path = "/Users/stlp/Documents/UW/DXARTS 499/Sounds/vaca_cega.wav"; // from freesound.org
~path = "/Users/stlp/Documents/UW/DXARTS 499/Field Recordings/FOA/SPTL023.WAV"; // personal field recording

// read buffer for sample playback
~buffer = Buffer.readChannel(s, ~path, channels: 0);

~buffer.plot; // verify that buffer contains signal

(
// playhead is a pointer to a sample in the buffer
~proxy.ar(\playheadPosition, 1); // define audio-rate proxy
~proxy[\playheadPosition] = 0; // start at beginning of sample

// reverb mod
~proxy.ar(\rotation, 1); // audio-rate proxy for rotation parameter
~proxy[\rotation] = 0; // start rotation at zero
)

// define output
(
~proxy[\out] = {
	var snd, snd2; // L and R channels
	var out;      // output
	var buf = ~buffer; // sound to process

	// read the contents of the input buffer at a given sample
	snd = BufRd.ar(1, buf, ~proxy[\playheadPosition] * BufFrames.kr(buf));
	snd2 = BufRd.ar(1, buf, ~proxy[\playheadPosition] * BufFrames.kr(buf) * LFNoise2.kr(0.1).range(0.99, 1.01));

	// apply reverb to output signal and control
	// wet/dry mix with parameter proxy
	FreeVerb2.ar(snd, snd2, mix: ~proxy[\rotation], room: 0.4);
}
)

~proxy[\out].free; // stop playback

// start sample playback
~proxy[\out].play;

~proxy[\out].scope; // view output

// map x-acc to playhead position
~proxy[\playheadPosition] = {Slew.ar(~proxy[\gyr].ar(1, 0, 0/*X axis*/).lag3(0.3).linlin(-1, 1, 0, 1), 1, 1)};

// map y-acc to rotation angle
~proxy[\rotation] = {Slew.ar(~proxy[\gyr].ar(1, 1, 0/*Y axis*/).lag3(0.3).linlin(-1, 1, 0, 1, clip: \min))};