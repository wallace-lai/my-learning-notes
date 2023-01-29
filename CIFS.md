
# 一、CIFS百科介绍
## CIFS功能
（1）访问服务器本地文件并读写这些文件

（2）与其它用户一起共享一些文件块

（3）在断线时自动恢复与网络的连接

（4）使用统一码（Unicode）文件名：文件名可以使用任何字符集，而不局限于为英语或西欧语言设计的字符集

## CIFS的优点
一般来说，CIFS使用户得到比FTP更好的对文件的控制。它提供潜在的更直接地服务器程序接口，这比使用HTTP协议的浏览器更好。CIFS最典型的应用是windows用户能够从“网上邻居”中找到网络中的其他主机并访问其中的共享文件夹。

CIFS 是开放的标准而且已经被作为Internet应用程序标准被提交到IETF。用于Java与DCom通信的开源第三方库J-Interop就是利用CIFS协议实现的。

## CIFS与NFS的对比
（1）CIFS面向网络连接的共享协议，对网络传输的可靠性要求高，常使用TCP/IP；NFS是独立于传输的，可使用TCP或UDP；

（2）NFS缺点之一，是要求client必须安装专用软件；而CIFS集成在OS 内部，无需额外添加软件；

（3）NFS属无状态协议，而CIFS属有状态协议；NFS受故障影响小，可以自恢复交互过程，CIFS不行；从传输效率上看，CIFS优于NFS，没用太多冗余信息传送；

（4）两协议都需要文件格式转换，NFS保留了unix的文件格式特性，如所有人、组等等；CIFS则完全按照win的风格来作。

【以上内容来自百度百科】

# 二、配置CIFS服务
## 安装SAMBA
```
sudo apt-get update;sudo apt-get install samba
```

## 配置smb.conf
配置前先备份
```
sudo cp /etc/samba/smb.conf /etc/samba/smb.conf.bak
```

在smb.conf文件的最后加上共享目录的配置信息如下，保存后退出
```
[CIFS]
   path = /home/yourname/CIFS
   available = yes
   browseable = yes
   public = yes
   writable = yes
```
【还有很多配置项没搞懂它的意思，后续再弄】

## 重启smbd服务
```
systemctl restart smbd.service
```

## 挂载共享目录CIFS
打开windows上的资源管理器，输入`\\192.168.xxx.xxx\CIFS`即可将设置好的`/home/yourname/CIFS`共享给挂载到windows上面

【以上内容来自网络，后续需要读一下smb.conf的man文档】


# 三、CIFS官方文档


# 四、CIFS常见操作的网络分析
## CIFS打开共享文件`\\10.32.106.72\dest\abc.txt`的过程
（1）由于CIFS是基于TCP的，所以第一步肯定是三次握手

（2）接下来第一个CIFS操作是协商，具体过程是客户端将自己所有支持的CIFS版本全部发送给服务器。服务器再从中选出自己能够支持的最高版本回复给客户端。协商的结果是确定客户端和服务器共同支持的最高版本的CIFS版本，后续传输就使用协商好的版本进行

（3）协商好版本后开始建立CIFS Session，建立Session的主要任务是进行身份验证，常用的验证方式有Kerberos和NTLM两种。Session建立之后就意味着客户端已经打开了服务器提供的CIFS共享`\\10.32.106.72`

（4）打开了共享`\\10.32.106.72`之后，要继续打开`\dest`共享。这个步骤被称为Tree Connect。Tree Connect完成之后服务器会返回一个Tree ID，后续的客户端将使用这个Tree ID去访问`\dest`共享的子目录和子文件（**类似open操作返回的fd，这意味着CIFS协议是有状态的，这是和NFS协议的一大差别之一**）

（5）Tree Connect步骤之后，在访问abc.txt之前还需要读取很多信息。包括文件的基本属性、标准属性、扩展属性、文件系统的信息等

（6）读取完信息后，接下来进行Create操作。无论是新建文件、打开目录、读写文件等过程都需要有Create操作。若用户没有操作文件的权限，则在Create步骤会收到“Access Denied”的错误提示

（7）最后是读取abc.txt操作

【以上内容来自Wireshark网络分析，缺少实践和图片，后续补上】


