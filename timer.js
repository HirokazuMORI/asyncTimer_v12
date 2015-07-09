var interval = 10;
var count1 = 0;
var count2 = 0;
var id;

id = setInterval(tim,interval);

function tim(res){
	if(global.gc){
		global.gc();
	}
	if(++count1 == 100){
		console.log(process.memoryUsage());
		count1=0;
		if(++count2 == 10){
			clearInterval(id);
			count2 = 0;
			id = setInterval(tim,interval);
		}
	}
}
