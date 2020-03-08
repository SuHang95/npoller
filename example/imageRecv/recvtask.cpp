#include<recv.h>
#include<stdio.h>
#include <fstream>

namespace Recv{
	recvtask::recvtask(const std::shared_ptr<tcp>& _conn,
		const logger& _log,recv* ret):
		conn(_conn),log(_log),receive(ret)
	{
		io_op ev=make_event(ReadEvent,conn->id(),this);
		if(conn->regist(ev)){
			state=1;
			//state=2;
		}
		else {
			log.info("Add a task on io instance %d fail!",conn->id());
			state=0;
		}

		func_load();
	}


	void recvtask::func_load(){
		state_func.emplace_back(
			[this](io_op& event){
				log.info("Task finished!");
			}
		);

		state_func.emplace_back(
			[this](io_op& event){
				switch(event.result){

					//if the last io_event was done
					case Done:{
						if(event.size<=event.__buff.size() && event.size!=0){
							
							if(event.size!=event.__buff.size())
								log.debug("Some error occur!,the size of event"
									" is not equal to io_buffer size %zd",
									event.__buff.size());

							frame ret=getframe(event.__buff,event.size);
							receive->addframe(ret);

							log.info("Got a frame,len=%zd",event.size);

							io_op ev=make_event(ReadEvent,conn->id(),this);
							if(conn->regist(ev)){
								state=1;
							}
							else {
								log.info("Add a task on io instance %d fail!",conn->id());
								state=0;
							}
						}
						else if(event.size==0){ 
							log.info("Have recieve a null packet!");
							state=1;
						}
						else{
							log.info("Recvtask error occur!");
							state=0;
						}
						break;
					}

					case IOError:{
						log.info("Recv task %zd fail,some error"
							"occur in fd %d",event.id,event.io_id);
						state=0;
						break;
					}

					case IOClose:{
						log.info("Print task %zd fail,the fd %d"
							"have closed!",event.id,event.io_id);
						state=0;
						break;
					}

					case WriteFlush:{
						log.info("Print task %zd in %zd fail,we"
							"have flush,but don't sure if it is "
							"send to kernel!",event.id,event.io_id);
						state=0;
						break;
					}
				}	
			}
 		);

	}

	
	void recvtask::notify(io_op& event){
		state_func[state.load()](event);
	}


}



int main(){
	Recv::recv _recv(3638);
	while(1){
		Recv::frame _frame;
		
		_frame=_recv.getframe();

		std::string name="./image/"+_frame.timestamp+".jpg";

		std::fstream image;
		image.open(name, std::ios_base::in | std::ios_base::out |
			 std::ios_base::binary | std::ios_base::app);
		if(image.is_open()){
			image.write(_frame.image.get(),_frame.image_len);
			image.flush();
			image.close();
		}else{
			Recv::debug.info("A file %s open fail!",
				name.c_str());
		}

		printf("Have got a frame,len=%d,x=%lf,y=%lf\n",_frame.image_len,_frame.x,_frame.y);
		Recv::debug.info("Have got a frame,len=%d,x=%lf,y=%lf,time=%s!\n",
			_frame.image_len,_frame.x,_frame.y,_frame.timestamp.c_str());
	}
}
