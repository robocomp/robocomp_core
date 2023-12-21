//
// Example: fps.print("FPS:");
// default refresh period is 1000

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/resource.h>

class FPSCounter
{
	public:
		FPSCounter()
		{
			begin = std::chrono::high_resolution_clock::now();
            FILE* file;
            struct tms timeSample;
            char line[128];

            lastCPU = times(&timeSample);
            lastSysCPU = timeSample.tms_stime;
            lastUserCPU = timeSample.tms_utime;

            file = fopen("/proc/cpuinfo", "r");
            numProcessors = 0;
            while(fgets(line, 128, file) != NULL){
                if (strncmp(line, "processor", 9) == 0) numProcessors++;
            }
            fclose(file);
		}
        int print( const std::string &text, const unsigned int msPeriod = 1000)
        {
            static int fps=0;
            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration<double>(end - begin).count() * 1000;
            if( elapsed > msPeriod)
            {
				last_period = elapsed/cont;
                float cpu = get_cpu_use();
                int mem = get_mem_use();
                std::cout << "Period = " << last_period << "ms. Fps = " << cont/(msPeriod/1000) << " " << text
                          << " cpu = " << cpu << "%" << " mem = " << mem << "MB" << std::endl;
                begin = std::chrono::high_resolution_clock::now();
                fps=cont;
                cont = 0;
            }
            cont++;
            return fps;
        }
		void print( const std::string &text, std::function<void(int)> f, const unsigned int msPeriod = 1000)
		{	
			auto end = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::duration<double>(end - begin).count() * 1000;
			if( elapsed > msPeriod)
			{
				last_period = elapsed/cont;
                float cpu = get_cpu_use();
                int mem = get_mem_use();
                std::cout << "Period = " << last_period << "ms. Fps = " << cont << " " << text
                          << " cpu = " << cpu << "%" << " mem = " << mem << "MB" << std::endl;
				begin = std::chrono::high_resolution_clock::now();
				f(cont);
				cont = 0;
			}
			cont++;
		}
		float get_period() const
        {
            return last_period;
        }

        float get_cpu_use()
        {
            struct tms timeSample;
            clock_t now;
            double percent;

            now = times(&timeSample);
            if (now <= lastCPU || timeSample.tms_stime < lastSysCPU ||
                timeSample.tms_utime < lastUserCPU)
            {
                //Overflow detection. Just skip this value.
                percent = -1.0;
            }
            else
            {
                percent = (timeSample.tms_stime - lastSysCPU) +
                          (timeSample.tms_utime - lastUserCPU);
                percent /= (now - lastCPU);
                percent *= 100;
            }
            lastCPU = now;
            lastSysCPU = timeSample.tms_stime;
            lastUserCPU = timeSample.tms_utime;
            return (int)percent;
        }
        int get_mem_use()
        { //Note: this value is in MB!
            FILE* file = fopen("/proc/self/status", "r");
            int result = -1;
            char line[128];

            while (fgets(line, 128, file) != NULL)
            {
                if (strncmp(line, "VmRSS:", 6) == 0)
                {
                    result = parseLine(line);
                    break;
                }
            }
            fclose(file);
            return result/1000;
        }
        int parseLine(char* line)
        {
            // This assumes that a digit will be found and the line ends in " Kb".
            int i = strlen(line);
            const char* p = line;
            while (*p <'0' || *p > '9') p++;
            line[i-3] = '\0';
            i = atoi(p);
            return i;
        }
		std::chrono::time_point<std::chrono::high_resolution_clock> begin;
		int cont = 0;
		float last_period = 0;
        clock_t lastCPU, lastSysCPU, lastUserCPU;
        int numProcessors;
};

