#ifndef _GRAPH_H_
#define _GRAPH_H_

#define MAX_IN 15
typedef struct
{
	long long timeStamp;
	long long myTimeStamp;
	std::vector<double> value;
} values;

typedef struct
{
	int			index;
	int			count;
	long long	timeStamp;
	double		data[1];
} message;


#endif
