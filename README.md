AntennaTuner
============
--------------------------------------------------------------------------------
__Automatic Antenna Tuner__  
__Filename:__ Antenna\_Tuner.ino  
__Date:__ 8 July 2012  
__Author:__ Dell-Ray Sackett | https://github.com/lospheris  
__Version:__ 0.5  
__Copyright (c) 2012 Dell-Ray Sackett__  
__License:__ License information can be found in the LICENSE.txt file which
  MUST accompany all versions of this code.  
__WARNING:__  
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,  
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF  
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE  
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  
__Discription:__  
  This is an automatic Antenna Tuner. Right now it is designed to work 
  with an ICOM-706. It will read the frequency from the radio and then
  adjust the antenna accordingly.  
__Required Nonstandard Libraries:__  
__LiquidTWI:__ https://github.com/Stephanie-Maks/Arduino-LiquidTWI  
__MemoryFree:__ http://arduino.cc/playground/Code/AvailableMemory  

--------------------------------------------------------------------------------
__Hardware__  
__Arduino Pinouts:__  
+ __Control Up:__ D5  
+ __Control Down:__ D4  
+ __Tune Button:__ D8  
+ __Relay A:__ D7  
+ __Relay B:__ D6  
+ __Reed Connection 1:__ 5Volts  
+ __Reed Connection 2:__ D3(int 1)  


