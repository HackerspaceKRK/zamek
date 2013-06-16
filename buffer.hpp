#include <string.h>

template<int length>
class CyclicBuffer {

	char buffer[length];
	unsigned int position;
	
public:

	CyclicBuffer() {
		position = 0;
	}

	inline void put(unsigned char value){
		buffer[position] = value;
		position = (position+1)%length;
	}

	inline char operator[](int index){
		return buffer[(position+index)%buffer];
	}
		
	inline void copy(char * target,unsigned int size, unsigned int offset = 0){
		for(int index = 0 ; index < length ; ++index){
			target[index] = buffer[(index+position+offset)%length];
		}
	}

	inline void clear(){
		memset(buffer,0,length*sizeof(unsigned char));
		position = 0;
	}

	inline int size(){
		return length;
	}
};
