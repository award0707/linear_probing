# Libraries
library(ggplot2)
library(ggrepel)
library(hrbrthemes)
library(latex2exp)

get_lf <- function(x){1-1/x}
rebuild_window <- function(n,x){n / (4 * x)}

x <- 1:1023
n <- 1

alpha <- get_lf(x)
R <- rebuild_window(n,x)

# create data
xValue <- x
yValue <- R
data <- data.frame(xValue,yValue)
keypoints <- subset(data, xValue %in% c(2^(0:10)))

# Plot
p <- ggplot(data, aes(x=xValue, y=yValue)) +
  geom_line() + 
  scale_x_continuous(trans='log2',breaks=c(2^(0:10))) +
  ggtitle("Rebuild window shrinks quickly") +
  labs(x = "Load factor parameter x", 
       y = "Rebuild window as a proportion of n") +
  theme_ipsum()
#round(get_lf(xValue),3)))
p + geom_point(data=keypoints) +
  geom_text_repel(data=keypoints, size=3.5, vjust=1, hjust=0.7,
    aes(label = format(round(get_lf(xValue),3),nsmall=3)))
