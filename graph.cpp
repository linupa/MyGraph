#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Value_Slider.H>

#define BUFLEN 1024
//#define SRV_IP "146.6.88.4"

static int win_width(500);
static int win_height(400);
static char const * win_title("Grapher");
static int head_info[5] = {0};
static int torso_info[3] = {0};
static int boundary[5] = {360, 360, 360, 400, 400};
static bool dirty = false;
static bool bPause = false;

using namespace std;

typedef struct 
{
	double value[10];
} values;

static vector<values> _value;


class TrackWindow : public Fl_Widget
{
public:
	TrackWindow(int xx, int yy, int width, int height);
	virtual ~TrackWindow();
	
	virtual void draw();

	void tick();
	static void timer_cb(void *param);
	
};

void *receive_udp(void *arg)
{
    struct sockaddr_in si_me, si_other;
    int s, slen;
	TrackWindow *pWidget = (TrackWindow *)arg;

    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        return NULL;

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(0xaa55);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(s, (const sockaddr*)&si_me, (socklen_t)sizeof(si_me));

    int buf[BUFLEN];
	values myValue;
    while (1)
    {
        recvfrom(s, buf, sizeof(int)*BUFLEN, 0, (sockaddr*)&si_other, (socklen_t*)&slen);
        fprintf(stderr, "PACKET RECEIVE... %4d,%4d,%4d,%4d,%4d\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
		for ( int i = 0 ; i < 10 ; i++ )
			myValue.value[i] = buf[i];
		
		if ( !bPause )
		{
			_value.push_back(myValue);
			dirty = true;
		}
    }
  
    return NULL;
}
TrackWindow::TrackWindow(int xx, int yy, int width, int height)
: Fl_Widget(xx, yy, width, height, "")
{
	Fl::add_timeout(0.01, timer_cb, this);
}


TrackWindow::~TrackWindow()  {
	Fl::remove_timeout(timer_cb, this);
}
 
void TrackWindow::
timer_cb(void * param)
{
	if ( dirty )
	{
		reinterpret_cast<TrackWindow*>(param)->redraw();
		dirty = false;
	}
	Fl::repeat_timeout(0.01, // gets initialized within tick()
			   timer_cb,
			   param);
}

#if 0
static bool paused_ready = false;
void TrackWindow::
timer_cb(void * param)
{
	static double lastZ = 0.0;

	if ( myModel.z == 0.0 && myModel.z == lastZ )
		paused = true;
	
	if ( !paused )
	{
		myModel.update();

		lastZ = myModel.z; 

		traceX.push_back(myModel.x);
		traceZ.push_back(myModel.z);

		reinterpret_cast<TrackWindow*>(param)->redraw();
	}
/*
	if ( ! paused || ! paused_ready ) {
		reinterpret_cast<Simulator*>(param)->tick();
	}

	if ( paused )
		paused_ready = true;
	if ( ! paused )
		paused_ready = false;
*/
	Fl::repeat_timeout(0.01, // gets initialized within tick()
			   timer_cb,
			   param);
}
#endif

  void TrackWindow::
  draw()
  {
    double scale;
    if (w() > h()) {
      scale = h() / 11.0;
    }
    else {
      scale = w() / 11.0;
    }
    double const x0(w() / 2.0);
    double const y0(h() / 4.0);
	double const y1(h() / 2.0);
	double const y2(h() *3.0 / 4.0);

    fl_color(FL_BLACK);
    fl_rectf(x(), y(), w(), h());

	int number = _value.size();
	int step = 5;

	int skip = 0;
	if (number*step > w() )
		skip = number - w()/step;
	values curr, prev;
    fl_line_style(FL_SOLID, 1, 0);
	int i = 0;
	for (vector<values>::iterator it = _value.begin(); it != _value.end() ; ++it )
	{
		curr = *it;
		if ( skip < i )
		{
			int x_p, x_c;	
			x_p = (i-1-skip)*step;
			x_c = x_p + step;
    		fl_color(FL_RED);
			fl_line(x_p, y0-prev.value[0], x_c, y0-curr.value[0]);
    		fl_color(FL_YELLOW);
			fl_line(x_p, y0-prev.value[1], x_c, y0-curr.value[1]);
    		fl_color(FL_BLUE);
			fl_line(x_p, y0-prev.value[2], x_c, y0-curr.value[2]);
    		fl_color(FL_RED);
			fl_line(x_p, y1-prev.value[3], x_c, y1-curr.value[3]);
    		fl_color(FL_YELLOW);
			fl_line(x_p, y1-prev.value[4], x_c, y1-curr.value[4]);
    		fl_color(FL_BLUE);
			fl_line(x_p, y1-prev.value[5], x_c, y1-curr.value[5]);
    		fl_color(FL_RED);
			fl_line(x_p, y2-prev.value[6], x_c, y2-curr.value[6]);
    		fl_color(FL_YELLOW);
			fl_line(x_p, y2-prev.value[7], x_c, y2-curr.value[7]);
    		fl_color(FL_BLUE);
			fl_line(x_p, y2-prev.value[8], x_c, y2-curr.value[8]);
		}
		prev = curr;
		i++;
	}	



#if 0
	fl_color(FL_YELLOW);
	vector<double>::iterator itx = traceX.begin();
	vector<double>::iterator itz = traceZ.begin();
	while ( itx != traceX.end() )
	{
		
		fl_point(x0+(*itx)*60, y0-(*itz)*60);
		itx++;
		itz++;
	}

    fl_color(FL_WHITE);
//	fl_point(x0+myModel.x*20, y0-myModel.z*20);
	fl_arc(x0+myModel.x*60, y0-myModel.z*60, 5, 5, 0.0, 360.0);
	printf("x    : %5f, z    : %5f\n", myModel.x, myModel.z);
/*
    fl_color(FL_WHITE);
    fl_line_style(FL_SOLID, 4, 0);
    raw_draw_tree(*sim_tree, x0, y0, scale);

    fl_color(FL_GREEN);
    fl_line_style(FL_SOLID, 10, 0);
    raw_draw_com(*sim_tree, x0, y0, scale);


    fl_line_style(0);       // back to default
*/
#endif
  }

class MyWindow : public Fl_Double_Window 
{
public:
	MyWindow(int width, int height, const char *title);

	virtual void resize(int x, int y, int w, int h);	
	Fl_Button *quit;
	Fl_Button *reset;
	Fl_Button *pause;
	Fl_Value_Slider *mSlider[5];
	TrackWindow *graph;
		
	static void cb_quit(Fl_Widget *widget, void *param);
	static void cb_reset(Fl_Widget *widget, void *param);
	static void cb_pause(Fl_Widget *widget, void *param);
//	static void cb_head(Fl_Widget *widget, void *param);
};

char *slider_name[] = {
	"Head_Roll",
	"Head_Yaw",
	"Head_Pitch",
	"Torso_Yaw",
	"Torso_Roll"
};

MyWindow:: 
MyWindow(int width, int height, const char * title) 
    : Fl_Double_Window(width, height, title) 
{ 
	int i;

    Fl::visual(FL_DOUBLE|FL_INDEX); 
    begin(); 
//    simulator = new Simulator(0, 0, width, height - 40); 
//    toggle = new Fl_Button(5, height - 35, 100, 30, "&Toggle"); 
//    toggle->callback(cb_toggle); 
//    pause = new Fl_Button(width / 2 - 50, height - 35, 100, 30, "&Pause"); 
//    pause->callback(cb_pause); 
	printf("WIN: %p\n", this);
    quit = new Fl_Button(width - 105, height - 35, 100, 30, "&Quit"); 
    quit->callback(cb_quit, this); 
	
    reset = new Fl_Button(width - 210, height - 35, 100, 30, "&Reset"); 
    reset->callback(cb_reset, this); 
	
    pause = new Fl_Button(width - 315, height - 35, 100, 30, "&Pause"); 
    pause->callback(cb_pause, this); 
	
	graph = new TrackWindow(10, 10, width - 20, height - 50 );

/*
	for ( i = 0 ; i < 5 ; i++ )
	{
		mSlider[i] = new Fl_Value_Slider( 30, 35 + 65*i, 200, 30, slider_name[i]);
		mSlider[i]->type(FL_HORIZONTAL);
		mSlider[i]->bounds(-boundary[i]/2., boundary[i]/2.);
		mSlider[i]->callback(cb_head, this);
	}
*/

    end(); 

    resizable(this); 

    show(); 
}

  void MyWindow::
  resize(int x, int y, int w, int h)
  {
    Fl_Double_Window::resize(x, y, w, h);
    quit->resize(w-105, h-35, 100, 30);
    reset->resize(w-210, h-35, 100, 30);
    pause->resize(w-315, h-35, 100, 30);
	graph->resize(10, 10, w-20, h-50);
  }


void MyWindow::
cb_reset(Fl_Widget *widget, void *param)
{
	_value.clear();
}
  
void MyWindow::
cb_pause(Fl_Widget *widget, void *param)
{
	bPause = !bPause;	
}
  
void MyWindow::
cb_quit(Fl_Widget *widget, void *param)
{
	reinterpret_cast<MyWindow*>(param)->hide();
}

#if 0
int s = 0;
struct sockaddr_in si_other;
void MyWindow::
cb_head(Fl_Widget *widget, void *param)
{
	int i;
	bool dirty = false;
	Fl_Value_Slider *pSlider = (Fl_Value_Slider *)widget;
	MyWindow *pWin = (MyWindow *)param;
	int value = pSlider->value();
	
	for ( i = 0 ; i < 5 ; i++ )
	{
		if ( (pSlider == pWin->mSlider[i]) && (head_info[i] != value) )
		{
			head_info[i] = value;
			dirty = true;
		}
	}

  	if ( dirty )
	{
		sendto(s, head_info, sizeof(head_info), 0, (const sockaddr*)&si_other, sizeof(si_other));
		printf("HEAD EVENT: %d (%d %d %d %d %d)\n", value, head_info[0], head_info[1], head_info[2], head_info[3], head_info[4]);
	}
}
#endif

int main(void)
{
  int i, slen=sizeof(si_other);
  char buf[BUFLEN];

  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
    return -1;

  pthread_t udp_receiver;


  values init_value;
  init_value.value[0] = 0;
  init_value.value[1] = 0;
  init_value.value[2] = 0; 
  _value.push_back(init_value);

  memset((char *) &si_other, 0, sizeof(si_other));
//  si_other.sin_family = AF_INET;
//  si_other.sin_port = htons(0xaa55);
//  si_other.sin_addr.s_addr = inet_addr(SRV_IP);

  MyWindow win(win_width, win_height, win_title);

  pthread_create( &udp_receiver, NULL, receive_udp, (void*)&win);

  int ret = Fl::run();

  close(s);

  return ret;
}
