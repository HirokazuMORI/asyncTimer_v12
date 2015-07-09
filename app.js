var asyncTimer = require('./build/Release/asyncTimer_v12.node');
var interval = 10;
var count = 0;

asyncTimer.start(tim,interval);

function tim(res){
	if(global.gc){
		global.gc();
	}
	if(++count == 100){
		console.log(process.memoryUsage());
		count=0;
	}
}
