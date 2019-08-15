#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <fcntl.h>

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Radio_Button.H>
#include <FL/Fl_Value_Slider.H>

#include "graph.h"

#define BUFLEN 1024
#define RGB(r,g,b) (((r)<<24)|((g)<<16)|((b)<<8))

#define IN_Y_CANVAS(x) ( ((x)>0) && ((x)<height) )

extern void *transform_f(void *arg);

int state = 0;

using namespace std;

static int win_width(500);
static int win_height(400);
static char const * win_title("Grapher");
static int head_info[5] = {0};
static int torso_info[3] = {0};
static int boundary[5] = {360, 360, 360, 400, 400};
static bool dirty = false;
static bool bPause = false;
bool gCheck[MAX_IN];
long num_input = 0;
long num_disp = 0;
double gScale = 1.;
double gOffset = 0.;
double gScroll = 100.;
double gTScale = 1.;
int   gPort = 0;
pthread_mutex_t data_mutex;
int   gSocketId = -1;

using namespace std;

vector<values> _value;

int colors[] = {
  RGB(255,0,0),
  RGB(0,255,0),
  RGB(0,255,255),
  RGB(255,255,0),
  RGB(255,255,255),
  RGB(255,0,255),
  RGB(255,100,100),
  RGB(100,255,100),
  RGB(100,100,255),
  RGB(255,255,100),
  RGB(100,255,255),
  RGB(255,100,255),
  RGB(255,0,0),
  RGB(0,255,0),
  RGB(0,255,255),
  RGB(255,255,0),
  RGB(255,255,255),
  RGB(255,0,255)
};

typedef struct
{
  pthread_mutex_t	*mutex;
  void *win;
  int portNum;
} udp_arg;


class TrackWindow : public Fl_Widget
{
public:
  TrackWindow(int xx, int yy, int width, int height);
  virtual ~TrackWindow();

  virtual void draw();

  void tick();
  static void timer_cb(void *param);
  int handle(int e);
};

int open_socket(int portNum)
{
  struct sockaddr_in si_me, si_other;
  int errno;

  if ( gSocketId != -1 )
    close(gSocketId);

  if ((gSocketId=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
    return -1;

  int flags;
    if (-1 == (flags = fcntl(gSocketId, F_GETFL, 0)))
        flags = 0;
    fcntl(gSocketId, F_SETFL, flags | O_NONBLOCK);

  memset((char *) &si_me, 0, sizeof(si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(portNum);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  if ( errno = bind(gSocketId, (const sockaddr*)&si_me, (socklen_t)sizeof(si_me)) )
  {
    fprintf(stderr, "Error on Binding... %d\n", errno );
  }


  fprintf(stderr, "Socket %d Opened...\n", portNum);

  return 0;
}

void close_socket(void)
{
  if ( gSocketId != -1 )
    close(gSocketId);
  gSocketId = -1;
}

void *receive_udp(void *arg)
{
  struct sockaddr_in si_other;
  int slen;
  pthread_mutex_t *mutex;

  udp_arg *Arg = (udp_arg *)arg;
  TrackWindow *pWidget = (TrackWindow *)Arg->win;
  mutex = Arg->mutex;
  free(arg);

  int length, index = 0;
  int num_buffer = MAX_IN;
  message *msg = (message *)malloc(sizeof(message) + sizeof(double)*(num_buffer-1));
  values myValue;
  myValue.value = vector<double>(num_buffer);

  fprintf(stderr, "RECV sock %d\n", gSocketId);
  fd_set rd_set;
  struct timeval tv;
  state = 1;
  open_socket( Arg->portNum );
  while (1)
  {
    FD_ZERO(&rd_set);
    FD_SET(gSocketId, &rd_set);

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    state = 2;
    select(FD_SETSIZE, &rd_set, NULL, NULL, &tv);

    state = 3;
    length = recvfrom(gSocketId, (void*)msg, sizeof(message) + sizeof(double) * (num_buffer - 1), 0, (sockaddr*)&si_other, (socklen_t*)&slen);
    state = 4;

    if ( length <= 0 )
    {
      fprintf(stderr, "Empty socket %d\n", length);
      continue;
    }
    state = 5;
    long long timeStamp = msg->timeStamp;
    num_input = msg->count;
    if ( num_buffer < num_input )
    {
      free(msg);
      num_buffer = num_input * 2;
      msg = (message *)malloc(sizeof(message) + sizeof(double)*(num_buffer-1));
      continue;
    }

    num_disp = min((int)num_input , (int)MAX_IN);

    myValue.timeStamp = timeStamp;
    if ( msg->index != (index+1) )
      fprintf(stderr, "Missing %d packets (%08x->%08x)\n", msg->index-index, msg->index, index );
    if ( myValue.value.size() < num_input )
      myValue.value = vector<double>(num_input);
    for ( int i = 0 ; i < num_input ; i++ )
      myValue.value[i] = msg->data[i];

    if ( !bPause )
    {
      pthread_mutex_lock(mutex);
      _value.push_back(myValue);
      dirty = true;
      pthread_mutex_unlock(mutex);
    }

    index = msg->index;
  }

  return NULL;
}

double target_pos[2];
double x_org;
double y_org;
double scale;
int point_pressed = -1;
double offset_base;
int TrackWindow::handle(int e)
{
  int ret = 0;
  int i;
  double posX, posY;
  static double x_pressed, y_pressed;

  posX = Fl::event_x();
  posY =  Fl::event_y();
#define CLICK_THRESHOLD 3

  switch (e)
  {
  case FL_PUSH:
    x_pressed = posX;
    y_pressed = posY;
    target_pos[0] = (posX - x_org) / scale;
    target_pos[1] = (y_org - posY) / scale;
    point_pressed = -1;
    for ( i = 0 ; i < 4 ; i++ )
    {
      double diffx, diffy;
      if ( diffx*diffx + diffy*diffy < CLICK_THRESHOLD*CLICK_THRESHOLD/scale/scale )
      {
        point_pressed = i;
        break;
      }
    }
    offset_base = gOffset;
    point_pressed = 1;
    fprintf(stderr, "PUSH EVENT!!! (%d)(%f,%f)\n",
      point_pressed, target_pos[0], target_pos[1]);
    ret = 1;
    break;
  case FL_DRAG:
    if ( point_pressed >= 0 )
    {
      fprintf(stderr, "DRAG EVENT!!! gOffset %f (%f,%f)\n", gOffset, y_pressed, posY);
      gOffset = offset_base + ( posY - y_pressed ) ;
    }

    break;
  case FL_RELEASE:
    fprintf(stderr, "RELEASE EVENT!!!\n");
    point_pressed = -1;
    break;
  case FL_ENTER:
    fprintf(stderr, "FOCUS EVENT!!!\n");
    ret = 1;
    break;
  case FL_MOUSEWHEEL:
    fprintf(stderr, "MOUSE WHEEL!!! %f\n", gScale);
    double current_value = (posY - gOffset) / gScale;
    gScale *= pow(1.1, (double)Fl::event_dy());

    gOffset = posY - current_value * gScale;

    ret = 1;
    break;
  }
  Fl_Widget::handle(e);

  return ret;
}
TrackWindow::TrackWindow(int xx, int yy, int width, int height)
: Fl_Widget(xx, yy, width, height, "")
{
  gOffset = height/2;
  Fl::add_timeout(0.1, timer_cb, this);
}


TrackWindow::~TrackWindow()  {
  Fl::remove_timeout(timer_cb, this);
}

void TrackWindow::
timer_cb(void * param)
{
  {
    reinterpret_cast<TrackWindow*>(param)->redraw();
    dirty = false;
  }

  Fl::repeat_timeout(0.1, // gets initialized within tick()
         timer_cb,
         param);
}

#define Y2SCR(y) ((int)(y0_scr - y*scale))
#define SCR2Y(yscr) ((double)(y0_scr - yscr)/scale)
void TrackWindow::
draw()
{
  int width  = w();
  int height = h();
  double const x0(width / 2.0);
  double const y0(height / 2.0);
  double const scale = gScale;
  double ymin, ymax;
  int ymin_scr, ymax_scr;
  int y0_scr = gOffset;
  double range;
  ymin_scr = 0;
  ymax_scr = height;
  ymin = (double)(-ymax_scr + y0_scr) / scale;
  ymax = (double)(-ymin_scr + y0_scr) / scale;

  range = ymax - ymin;

  if ( bPause )
    fl_color(RGB(30,30,30));
  else
    fl_color(FL_BLACK);

  fl_rectf(x(), y(), width, height);

  int number = _value.size();
  double tscale = gTScale;
  double step = 1./tscale;
  int skip = 0;
  int sample_number = width/step + 1;

  skip = number - sample_number;
  skip = (int)((double)skip * gScroll / 100.);
  if ( skip < 0 )
    skip = 0;
  values curr, prev;
    fl_line_style(FL_SOLID, 1, 0);
  int i = 0;
  double j = 0;
  double unit;
  int unit_scr;

  int log_scale = (int)((double)log(range) / (double)log(10));

  unit = pow(10, log_scale-2);
  int count = range / unit;
  if (count > 500)
    unit *= 50;
  else if (count > 200)
    unit *= 20;
  else if (count > 100)
    unit *= 10;
  else if (count > 50)
    unit *= 5;
  else if (count>20)
    unit *= 2;

  unit_scr = (double)unit*scale;

  double unit_base;
  int unit_base_scr;

  unit_base = (double)((int)(ymin / unit) * unit);
  unit_base_scr = Y2SCR(unit_base);

  fl_color(RGB(40,40,40));
  double tic;
  for ( tic = unit_base ; tic < ymax ; tic += unit )
  {
    fl_line(x(), Y2SCR(tic), width, Y2SCR(tic));
  }

  fl_color(RGB(255,255,255));
  for ( tic = unit_base ; tic < ymax ; tic += unit )
  {
    char buf[20];
    const char *form1 = "%+.2lf";
    const char *form2 = "%+.5lf";
    if (unit > 0.01)
      sprintf(buf, form1, tic);
    else
      sprintf(buf, form2, tic);
    fl_draw(buf, x(), Y2SCR(tic));
  }

  fl_color(RGB(80,80,80));
  fl_line(x(), y0_scr, width, y0_scr);

  pthread_mutex_lock(&data_mutex);
  vector<values>::iterator it = _value.begin();
  vector<values>::iterator end = _value.end();
  it += skip;

  for ( i = 0 ; it < end ; ++i )
  {
    curr = *it;
    int x_p, x_c;
    int j;

    if ( step >= 1. )
    {
      x_p = (i-1)*step;
      x_c = x_p + step;

      for ( j = 0 ; j < num_disp ; j++ )
      {
        if ( x_p < 0 )
          break;
        if ( !gCheck[j] )
          continue;
        double y_p = Y2SCR(prev.value[j]);
        double y_c = Y2SCR(curr.value[j]);

        if (!IN_Y_CANVAS(y_p))
          continue;
        if (!IN_Y_CANVAS(y_c))
          continue;

        fl_color(colors[j]);
        fl_line(x_p, y_p, x_c, y_c);
      }
      prev = curr;
      ++it;
    }
    else
    {
      int num;
      x_p = i-1;
      x_c = i;

      num = (tscale >= end-it)?(end-it):(tscale);
      for ( j = 0 ; j < num_disp ; j++ )
      {
        if ( x_p < 0 )
          break;
        if ( !gCheck[j] )
          continue;
        int k;
        double y_max = curr.value[j], y_min = curr.value[j];
        for ( k = 0 ; k < num ; k++ )
        {
          double y = (it+k)->value[j];

          if ( y > y_max )
            y_max = y;
          else if ( y < y_min )
            y_min = y;
        }

        y_min = Y2SCR(y_min);
        y_max = Y2SCR(y_max);
        double y_p = Y2SCR(prev.value[j]);
        double y_c = Y2SCR(curr.value[j]);

        fl_color(colors[j]);
        fl_line(x_p, y_p, x_c, y_c);
        fl_line(x_c, y_min, x_c, y_max);

      }
      prev = *(it+tscale-1);
      it += tscale;
    }
  }

  {
    static int count = 0;

    if ( (count % 100) == 0 )
      fprintf(stderr, "Step %f\n", step);
    ++count;
  }

  pthread_mutex_unlock(&data_mutex);
}

class MyWindow : public Fl_Double_Window
{
public:
  MyWindow(int width, int height, const char *title);

  virtual void resize(int x, int y, int w, int h);
  Fl_Button *quit;
  Fl_Button *save;
  Fl_Button *reset;
  Fl_Button *pause;
  Fl_Value_Slider *mScale;
  Fl_Value_Slider *mScroll;
  Fl_Value_Slider *mTScale;
  TrackWindow *graph;
  Fl_Check_Button *mCheck[MAX_IN+1];

  static void cb_quit(Fl_Widget *widget, void *param);
  static void cb_save(Fl_Widget *widget, void *param);
  static void cb_reset(Fl_Widget *widget, void *param);
  static void cb_pause(Fl_Widget *widget, void *param);
  static void cb_scale(Fl_Widget *widget, void *param);
  static void cb_tscale(Fl_Widget *widget, void *param);
  static void cb_scroll(Fl_Widget *widget, void *param);
  static void cb_check(Fl_Widget *widget, void *param);
  static void cb_freq(Fl_Widget *widget, void *param);
  static void cb_port(Fl_Widget *widget, void *param);
};

MyWindow::
MyWindow(int width, int height, const char * title)
    : Fl_Double_Window(width, height, title)
{
  int i;

  Fl::visual(FL_DOUBLE|FL_INDEX);
  begin();

  fprintf(stderr,"WIN: %p\n", this);
  quit = new Fl_Button(width - 75, height - 35, 70, 30, "&Quit");
  quit->callback(cb_quit, this);

  save = new Fl_Button(width - 150, height - 35, 70, 30, "&Save");
  save->callback(cb_save, this);

  reset = new Fl_Button(width - 225, height - 35, 70, 30, "&Reset");
  reset->callback(cb_reset, this);

  pause = new Fl_Button(width - 300, height - 35, 70, 30, "&Pause");
  pause->callback(cb_pause, this);

  graph = new TrackWindow(10, 10, width - 120, height - 20 );

  mScale = new Fl_Value_Slider( 5, height - 50, (width - 350)/2 - 2, 20, "");
  mScale->type(FL_HORIZONTAL);
  mScale->bounds(-5., 5.);
  mScale->callback(cb_scale, this);

  mScroll = new Fl_Value_Slider( 5, height - 25, width - 350, 20, "");
  mScroll->type(FL_HORIZONTAL);
  mScroll->bounds(0., 100.);
  mScroll->value(100.);
  mScroll->callback(cb_scroll, this);

  mTScale = new Fl_Value_Slider( 5 + (width - 350)/2 - 2 + 5, height - 50, (width - 350)/2 - 3, 20, "");
  mTScale->type(FL_HORIZONTAL);
  mTScale->bounds(-2., 2.);
  mTScale->value(0.);
  mTScale->callback(cb_tscale, this);

  for ( i = 0 ; i < MAX_IN+1 ; i++ )
  {
    char *pLabel;
    pLabel = (char *)malloc(5);
    sprintf(pLabel, "%2d", i);
    mCheck[i] = new Fl_Check_Button(width - 40, 30 + 30 * i, 30, 30, (i==0)?"All":pLabel);
    mCheck[i]->callback(cb_check, this);
    mCheck[i]->set();
    gCheck[i%MAX_IN] = true;
  }
  end();

  resizable(this);

  show();
}

void MyWindow::
resize(int x, int y, int w, int h)
{
  Fl_Double_Window::resize(x, y, w, h);
  quit->resize(w-75, h-35, 70, 30);
  save->resize(w-150, h-35, 70, 30);
  reset->resize(w-225, h-35, 70, 30);
  pause->resize(w-300, h-35, 70, 30);
  graph->resize(10, 10, w-60, h-65);
  mScale->resize( 5, h - 50, (w - 380)/2-2, 20);
  mScroll->resize( 5, h - 25, w - 380, 20);
  mTScale->resize( 5 + (w-380)/2-2 + 5, h - 50, (w - 380)/2-3, 20);
  for ( int i = 0 ; i < MAX_IN+1 ; i++ )
    mCheck[i]->resize(w - 40, 30 + 30 * i, 30, 30);
}

void MyWindow::
cb_reset(Fl_Widget *widget, void *param)
{
  dirty = true;
  _value.clear();
}

void MyWindow::
cb_pause(Fl_Widget *widget, void *param)
{
  dirty = true;
  bPause = !bPause;

}

void MyWindow::
cb_save(Fl_Widget *widget, void *param)
{
  time_t now;
  struct tm *local;
  char buff[255];

  time(&now);

  local = localtime(&now);

  sprintf(buff, "GRAPH_%04d-%02d-%02d_%02d:%02d:%02d.log", local->tm_year+1900, local->tm_mon+1, local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec);
  fprintf(stderr, "File: %s\n", buff);

  FILE *pOut;

  pOut = fopen(buff, "w");

  if ( pOut == NULL )
    return;

  for (vector<values>::iterator it = _value.begin(); it != _value.end() ; ++it )
  {
    values curr = *it;
    int j;
    fprintf(pOut, "%lld ", curr.timeStamp);
    for ( int j = 0 ; j < num_input ; j++ )
    {
      fprintf(pOut, "%f ", curr.value[j]);
    }
    fprintf(pOut, "\n");
  }
  fclose(pOut);
}

void MyWindow::
cb_quit(Fl_Widget *widget, void *param)
{
  reinterpret_cast<MyWindow*>(param)->hide();
}

void MyWindow::
cb_scale(Fl_Widget *widget, void *param)
{
  int i;
  dirty = true;
  Fl_Value_Slider *pSlider = (Fl_Value_Slider *)widget;
  MyWindow *pWin = (MyWindow *)param;
  double value = pSlider->value();

  gScale = pow(10., value);
  fprintf(stderr, "Scale Changed 10^%f = %f\n", value, gScale);

}

void MyWindow::
cb_tscale(Fl_Widget *widget, void *param)
{
  int i;
  dirty = true;
  Fl_Value_Slider *pSlider = (Fl_Value_Slider *)widget;
  MyWindow *pWin = (MyWindow *)param;
  double value = pSlider->value();

  gTScale = pow(10., value);

}

void MyWindow::
cb_scroll(Fl_Widget *widget, void *param)
{
  int i;
  dirty = true;
  Fl_Value_Slider *pSlider = (Fl_Value_Slider *)widget;
  MyWindow *pWin = (MyWindow *)param;
  double value = pSlider->value();

  gScroll = value;
}

void MyWindow::
cb_check(Fl_Widget *widget, void *param)
{
  int i;
  dirty = true;
  Fl_Check_Button *pButton = (Fl_Check_Button *)widget;
  MyWindow *pWin = (MyWindow *)param;
  int value = pButton->value();
  int index = -1;

  while ( pButton != pWin->mCheck[++index] );

  cout << "INDEX: " << index << " VALUE: " << value << endl;
  if ( index != 0 )
  {
    gCheck[index-1] = value;
  }
  else for ( i = 0 ; i < MAX_IN ; i++ )
  {
    pWin->mCheck[i+1]->value(value);
    gCheck[i] = value;
  }
}

int main(int argc, const char *argv[])
{
  char buf[BUFLEN];
  pthread_t udp_receiver;

  if (argc  < 2 )
  {
    fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
    exit(-1);
  }

  MyWindow win(win_width, win_height, win_title);

  pthread_mutex_init( &data_mutex, NULL );

  udp_arg *arg = (udp_arg *)malloc(sizeof(udp_arg));
  arg->mutex = &data_mutex;
  arg->win = (void *)&win;
  arg->portNum = atoi(argv[1]);
  pthread_create( &udp_receiver, NULL, receive_udp, (void*)arg);

  int ret = Fl::run();

  close_socket();

  return ret;
}

