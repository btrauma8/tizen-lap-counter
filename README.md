# tizen-lap-counter

A web app ui for tizen 5.5 that uses a C service to do a few things in the background (vibrate, check gps location).

I couldn't get the web app "get location in background, vibrate in background" to work.

So, the web app and c service chat back and forth.

c service sends lat longs, does the vibrating.

I tried and failed to implement a web service to do this stuff in the background...woulda been so much easier.

So...it's a c service.

The point of the app is to count laps on a trail that I run on. I define a polygon of lat longs. If gps puts me within that polygon, increment lap...don't increment again for x seconds. vibrate phone every lap. Simple.
