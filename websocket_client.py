#!/usr/bin/env python3
"""
WebSocket客户端，用于调用语音识别服务端
支持麦克风实时推理和文件推理
"""
import asyncio
import websockets
import numpy as np
import sys
import os
# import logging
from loguru import logger
import argparse
import json
from pathlib import Path
import time

try:
    import librosa
    import soundfile as sf
    LIBROSA_AVAILABLE = True
except ImportError:
    LIBROSA_AVAILABLE = False
    print("Warning: librosa不可用，只支持WAV文件格式")

try:
    import pyaudio
    PYAUDIO_AVAILABLE = True
except ImportError:
    PYAUDIO_AVAILABLE = False
    print("Warning: pyaudio不可用，无法使用麦克风")

import wave


def load_audio_file(audio_file_path, target_sr=16000):
    """
    加载音频文件，支持多种格式
    返回: (audio_data, sample_rate) 其中audio_data是单声道float32数据，范围[-1, 1]
    """
    file_path = Path(audio_file_path)
    if not file_path.exists():
        raise FileNotFoundError(f"音频文件不存在: {audio_file_path}")
    
    file_ext = file_path.suffix.lower()
    logger.info(f"加载音频文件: {file_path} (格式: {file_ext})")
    
    # 优先使用librosa（质量最好，支持最多格式）
    if LIBROSA_AVAILABLE:
        try:
            logger.info("使用librosa加载音频...")
            audio_data, sample_rate = librosa.load(
                audio_file_path, 
                sr=target_sr,  # 自动重采样到目标采样率
                mono=True,     # 转换为单声道
                dtype=np.float32
            )
            logger.info(f"librosa加载成功: {sample_rate}Hz, {len(audio_data)}个采样点, 范围: [{audio_data.min():.3f}, {audio_data.max():.3f}]")
            return audio_data, sample_rate
        except Exception as e:
            logger.error(f"librosa加载失败: {e}")
    
    # 回退到wave库处理WAV文件
    if file_ext == '.wav':
        try:
            logger.info("使用wave库加载WAV文件...")
            with wave.open(audio_file_path, 'rb') as wav_file:
                frames = wav_file.readframes(wav_file.getnframes())
                sample_rate = wav_file.getframerate()
                channels = wav_file.getnchannels()
                sample_width = wav_file.getsampwidth()
                
            logger.info(f"WAV文件信息: {sample_rate}Hz, {channels}声道, {sample_width}字节/采样")
            
            # 转换为float32格式
            if sample_width == 2:  # 16-bit
                audio_data = np.frombuffer(frames, dtype=np.int16).astype(np.float32) / 32768.0
            elif sample_width == 4:  # 32-bit
                audio_data = np.frombuffer(frames, dtype=np.int32).astype(np.float32) / 2147483648.0
            else:
                raise ValueError(f"不支持的采样位宽: {sample_width}")
                
            # 如果是立体声，转换为单声道
            if channels == 2:
                audio_data = audio_data.reshape(-1, 2).mean(axis=1)
                
            # 如果需要重采样
            if sample_rate != target_sr:
                if LIBROSA_AVAILABLE:
                    audio_data = librosa.resample(audio_data, orig_sr=sample_rate, target_sr=target_sr)
                    sample_rate = target_sr
                else:
                    # 简单降采样
                    step = max(1, sample_rate // target_sr)
                    audio_data = audio_data[::step]
                    sample_rate = target_sr
                    
            logger.info(f"WAV处理完成: {sample_rate}Hz, {len(audio_data)}个采样点, 范围: [{audio_data.min():.3f}, {audio_data.max():.3f}]")
            return audio_data, sample_rate
            
        except Exception as e:
            logger.error(f"WAV文件加载失败: {e}")
    
    raise ValueError(f"不支持的音频格式: {file_ext}. 支持的格式: WAV (推荐安装librosa以支持更多格式)")


class MicrophoneStream:
    """实时麦克风音频流"""
    
    def __init__(self, sample_rate=16000, chunk_size=1024):
        if not PYAUDIO_AVAILABLE:
            raise ImportError("需要安装pyaudio才能使用麦克风")
        
        self.target_sample_rate = sample_rate
        self.actual_sample_rate = sample_rate
        self.chunk_size = chunk_size
        self.format = pyaudio.paFloat32
        self.channels = 1
        self.p = None
        self.stream = None
        self.device_index = None
        
    def _find_best_device(self):
        """找到最佳的音频输入设备"""
        # 优先级：pipewire > pulse > USB音频设备 > 其他
        device_priorities = [
            ("pipewire", 100),
            ("pulse", 90),
            ("Razer Seiren", 80),
            ("USB Audio", 70),
            ("Microphone", 60),
            ("Line Input", 50)
        ]
        
        best_device = None
        best_score = -1
        default_device = self.p.get_default_input_device_info()
        
        logger.info("可用的音频输入设备:")
        for i in range(self.p.get_device_count()):
            info = self.p.get_device_info_by_index(i)
            if info['maxInputChannels'] > 0:
                logger.info(f"  {i}: {info['name']} (最大输入声道: {info['maxInputChannels']}, 默认采样率: {info['defaultSampleRate']})")
                
                # 计算设备评分
                score = 0
                for keyword, priority in device_priorities:
                    if keyword.lower() in info['name'].lower():
                        score = priority
                        break
                
                # 如果是默认设备，加分
                if i == default_device['index']:
                    score += 10
                    logger.info(f"    ^ 这是系统默认输入设备")
                
                if score > best_score:
                    best_score = score
                    best_device = i
        
        return best_device, default_device
        
    def _test_sample_rate(self, device_index, sample_rate):
        """测试设备是否支持指定采样率"""
        try:
            # 尝试打开流但不启动
            test_stream = self.p.open(
                format=self.format,
                channels=self.channels,
                rate=sample_rate,
                input=True,
                input_device_index=device_index,
                frames_per_buffer=self.chunk_size,
                start=False
            )
            test_stream.close()
            return True
        except Exception:
            return False
            
    def _find_best_sample_rate(self, device_index):
        """为设备找到最佳采样率"""
        # 常见采样率，优先选择目标采样率
        sample_rates = [self.target_sample_rate, 44100, 48000, 22050, 32000, 8000]
        
        for rate in sample_rates:
            if self._test_sample_rate(device_index, rate):
                logger.info(f"设备支持采样率: {rate}Hz")
                return rate
        
        # 如果都不支持，尝试设备的默认采样率
        try:
            device_info = self.p.get_device_info_by_index(device_index)
            default_rate = int(device_info['defaultSampleRate'])
            if self._test_sample_rate(device_index, default_rate):
                logger.info(f"使用设备默认采样率: {default_rate}Hz")
                return default_rate
        except Exception:
            pass
        
        raise RuntimeError("无法找到支持的采样率")
        
    def start(self):
        """开始录音"""
        self.p = pyaudio.PyAudio()
        
        # 找到最佳设备
        best_device, default_device = self._find_best_device()
        
        if best_device is not None:
            self.device_index = best_device
            device_name = self.p.get_device_info_by_index(best_device)['name']
            logger.info(f"选择音频设备: {device_name} (索引: {best_device})")
        else:
            self.device_index = default_device['index']
            logger.info(f"使用默认音频设备: {default_device['name']} (索引: {self.device_index})")
        
        # 找到最佳采样率
        try:
            self.actual_sample_rate = self._find_best_sample_rate(self.device_index)
        except RuntimeError as e:
            logger.error(f"采样率配置失败: {e}")
            # 尝试使用默认设备
            if self.device_index != default_device['index']:
                logger.info("尝试使用默认设备...")
                self.device_index = default_device['index']
                try:
                    self.actual_sample_rate = self._find_best_sample_rate(self.device_index)
                except RuntimeError:
                    raise RuntimeError("无法配置任何可用的音频设备")
            else:
                raise
        
        # 打开音频流
        try:
            self.stream = self.p.open(
                format=self.format,
                channels=self.channels,
                rate=self.actual_sample_rate,
                input=True,
                input_device_index=self.device_index,
                frames_per_buffer=self.chunk_size
            )
            
            logger.info(f"录音已启动: {self.actual_sample_rate}Hz, {self.channels}声道")
            if self.actual_sample_rate != self.target_sample_rate:
                logger.warning(f"注意: 实际采样率({self.actual_sample_rate}Hz) != 目标采样率({self.target_sample_rate}Hz), 将进行重采样")
                
        except Exception as e:
            logger.error(f"打开音频流失败: {e}")
            raise
        
    def read(self):
        """读取一个音频块"""
        if not self.stream:
            raise RuntimeError("音频流未启动")
        
        data = self.stream.read(self.chunk_size, exception_on_overflow=False)
        audio_data = np.frombuffer(data, dtype=np.float32)
        
        # 如果需要重采样
        if self.actual_sample_rate != self.target_sample_rate:
            if LIBROSA_AVAILABLE:
                # 使用librosa进行高质量重采样
                audio_data = librosa.resample(
                    audio_data, 
                    orig_sr=self.actual_sample_rate, 
                    target_sr=self.target_sample_rate
                )
            else:
                # 简单重采样
                ratio = self.target_sample_rate / self.actual_sample_rate
                new_length = int(len(audio_data) * ratio)
                if new_length > 0:
                    indices = np.linspace(0, len(audio_data) - 1, new_length)
                    audio_data = np.interp(indices, np.arange(len(audio_data)), audio_data)
        
        return audio_data
        
    @property
    def sample_rate(self):
        """返回目标采样率"""
        return self.target_sample_rate
        
    def stop(self):
        """停止录音"""
        if self.stream:
            self.stream.stop_stream()
            self.stream.close()
            self.stream = None
        if self.p:
            self.p.terminate()
            self.p = None
        logger.info("录音已停止")


class ASRWebSocketClient:
    """语音识别WebSocket客户端"""
    
    def __init__(self, uri, sample_rate=16000):
        self.uri = uri
        self.sample_rate = sample_rate
        self.websocket = None
        
    async def connect(self):
        """连接到WebSocket服务器"""
        try:
            # 在URI中添加采样率参数
            connect_uri = f"{self.uri}?samplerate={self.sample_rate}"
            self.websocket = await websockets.connect(connect_uri)
            logger.info(f"已连接到 {connect_uri}")
            return True
        except Exception as e:
            logger.error(f"连接失败: {e}")
            return False
            
    async def disconnect(self):
        """断开连接"""
        if self.websocket:
            await self.websocket.close()
            self.websocket = None
            logger.info("已断开连接")
            
    async def send_audio_chunk(self, audio_data):
        """发送音频数据块"""
        if not self.websocket:
            raise RuntimeError("未连接到服务器")
        
        # 确保数据是float32格式，然后转换为int16再发送
        if audio_data.dtype != np.float32:
            audio_data = audio_data.astype(np.float32)
        
        # 转换为int16格式（服务器期待的格式）
        audio_int16 = (audio_data * 32767).astype(np.int16)
        audio_bytes = audio_int16.tobytes()
        
        await self.websocket.send(audio_bytes)
        
    async def send_control_message(self, command):
        """发送控制消息（用于oneshot接口）"""
        if not self.websocket:
            raise RuntimeError("未连接到服务器")
        
        control_msg = {
            "command": command
        }
        await self.websocket.send(json.dumps(control_msg))
        logger.debug(f"发送控制消息: {command}")

    async def receive_result(self):
        """接收识别结果"""
        if not self.websocket:
            return None
        
        try:
            response = await asyncio.wait_for(self.websocket.recv(), timeout=0.1)
            result = json.loads(response)
            return result
        except asyncio.TimeoutError:
            return None
        except json.JSONDecodeError as e:
            logger.warning(f"JSON解析错误: {e}, 原始数据: {response}")
            return None
        except Exception as e:
            logger.error(f"接收结果错误: {e}")
            return None
            
    async def process_file(self, audio_file_path):
        """处理音频文件"""
        logger.info(f"开始处理文件: {audio_file_path}")
        
        # 加载音频文件
        try:
            audio_data, file_sample_rate = load_audio_file(audio_file_path, self.sample_rate)
        except Exception as e:
            logger.error(f"加载音频文件失败: {e}")
            return
        
        if not await self.connect():
            return
            
        try:
            # 创建接收结果的任务
            async def receive_results():
                results = []
                while True:
                    result = await self.receive_result()
                    if result is None:
                        await asyncio.sleep(0.01)
                        continue
                    
                    results.append(result)
                    if result.get('finished', False):
                        logger.info(f"✓ [完成] 片段 {result.get('idx', 0)}: {result}")
                    else:
                        logger.info(f"→ [进行中] 片段 {result.get('idx', 0)}: {result}")
                return results
            
            # 启动接收任务
            receive_task = asyncio.create_task(receive_results())
            
            # 分块发送音频数据
            chunk_size = self.sample_rate // 10  # 100ms 块
            total_chunks = (len(audio_data) + chunk_size - 1) // chunk_size
            
            logger.info(f"开始发送音频数据，共 {total_chunks} 个块，每块 {chunk_size} 个采样点")
            
            for i in range(0, len(audio_data), chunk_size):
                chunk = audio_data[i:i+chunk_size]
                await self.send_audio_chunk(chunk)
                
                chunk_num = i // chunk_size + 1
                logger.debug(f"已发送块 {chunk_num}/{total_chunks}")
                
                # 模拟实时流，适当延迟
                await asyncio.sleep(0.1)
            
            logger.info("音频发送完成，等待最终结果...")
            
            # 等待一段时间以接收所有结果
            await asyncio.sleep(3.0)
            receive_task.cancel()
            
        except Exception as e:
            logger.error(f"处理文件时发生错误: {e}")
        finally:
            await self.disconnect()
            
    async def process_microphone(self, duration=None):
        """处理麦克风输入"""
        logger.info("开始麦克风识别")
        
        if not await self.connect():
            return
            
        mic_stream = MicrophoneStream(self.sample_rate)
        
        try:
            mic_stream.start()
            
            # 创建接收结果的任务
            async def receive_results():
                while True:
                    result = await self.receive_result()
                    if result is None:
                        await asyncio.sleep(0.01)
                        continue
                    
                    if result.get('finished', False):
                        logger.info(f"✓ [完成] 片段 {result.get('idx', 0)}: {result.get('text', '')}")
                    else:
                        logger.info(f"→ [进行中] 片段 {result.get('idx', 0)}: {result.get('text', '')}")
            
            # 启动接收任务
            receive_task = asyncio.create_task(receive_results())
            
            # 录音和发送音频数据
            start_time = time.time()
            chunk_count = 0
            
            if duration:
                logger.info(f"录音 {duration} 秒...")
            else:
                logger.info("开始录音，按 Ctrl+C 停止...")
            
            try:
                while True:
                    if duration and time.time() - start_time > duration:
                        break
                        
                    # 读取音频块
                    audio_chunk = mic_stream.read()
                    await self.send_audio_chunk(audio_chunk)
                    
                    chunk_count += 1
                    if chunk_count % 50 == 0:  # 每5秒打印一次
                        elapsed = time.time() - start_time
                        logger.debug(f"已录音 {elapsed:.1f} 秒")
                    
                    await asyncio.sleep(0.01)  # 小延迟
                    
            except KeyboardInterrupt:
                logger.info("收到停止信号")
            
            logger.info("录音结束，等待最终结果...")
            await asyncio.sleep(2.0)
            receive_task.cancel()
            
        except Exception as e:
            logger.error(f"麦克风处理错误: {e}")
        finally:
            mic_stream.stop()
            await self.disconnect()

    async def process_file_oneshot(self, audio_file_path):
        """使用OneShot接口处理音频文件"""
        logger.info(f"开始使用OneShot接口处理文件: {audio_file_path}")
        
        # 加载音频文件
        try:
            audio_data, file_sample_rate = load_audio_file(audio_file_path, self.sample_rate)
        except Exception as e:
            logger.error(f"加载音频文件失败: {e}")
            return
        
        # 将URI修改为oneshot端点
        original_uri = self.uri
        if "/sttRealtime" in self.uri:
            self.uri = self.uri.replace("/sttRealtime", "/oneshot")
        elif not "/oneshot" in self.uri:
            # 如果URI中没有具体端点，添加oneshot
            self.uri = self.uri.rstrip('/') + '/oneshot'
        
        if not await self.connect():
            self.uri = original_uri  # 恢复原URI
            return
            
        try:
            # 创建接收结果的任务
            async def receive_results():
                results = []
                while True:
                    result = await self.receive_result()
                    if result is None:
                        await asyncio.sleep(0.01)
                        continue
                    
                    results.append(result)
                    result_type = result.get('type', 'unknown')
                    
                    if result_type == 'status':
                        status = result.get('status', '')
                        logger.info(f"📟 [状态] {status}")
                    elif result_type == 'result':
                        text = result.get('text', '')
                        lang = result.get('lang', 'auto')
                        emotion = result.get('emotion', '')
                        event = result.get('event', '')
                        timestamps = result.get('timestamps', [])
                        tokens = result.get('tokens', [])
                        
                        logger.info(f"✅ [识别结果] 文本: {text}")
                        if lang != 'auto':
                            logger.info(f"    语言: {lang}")
                        if emotion:
                            logger.info(f"    情感: {emotion}")
                        if event:
                            logger.info(f"    事件: {event}")
                        if timestamps:
                            logger.info(f"    时间戳: {len(timestamps)}个")
                        if tokens:
                            logger.info(f"    词元: {', '.join(tokens)}")
                    elif result_type == 'error':
                        error_msg = result.get('message', '未知错误')
                        logger.error(f"❌ [错误] {error_msg}")
                    else:
                        logger.info(f"📨 [其他消息] {result}")
                return results
            
            # 启动接收任务
            receive_task = asyncio.create_task(receive_results())
            
            # 发送开始录音命令
            await self.send_control_message("start")
            await asyncio.sleep(0.5)  # 等待服务器准备
            
            # 分块发送音频数据
            chunk_size = self.sample_rate # // 10  # 100ms 块
            total_chunks = (len(audio_data) + chunk_size - 1) // chunk_size
            
            logger.info(f"开始发送音频数据，共 {total_chunks} 个块，每块 {chunk_size} 个采样点")
            
            for i in range(0, len(audio_data), chunk_size):
                chunk = audio_data[i:i+chunk_size]
                await self.send_audio_chunk(chunk)
                
                chunk_num = i // chunk_size + 1
                logger.debug(f"已发送块 {chunk_num}/{total_chunks}")
                
                # 适当延迟模拟实时发送
                await asyncio.sleep(0.05)
            
            logger.info("音频发送完成，发送停止命令...")
            
            # 发送停止录音命令，开始处理
            await self.send_control_message("stop")
            
            # 等待处理结果
            logger.info("等待识别结果...")
            await asyncio.sleep(5.0)
            receive_task.cancel()
            
        except Exception as e:
            logger.error(f"OneShot处理文件时发生错误: {e}")
        finally:
            await self.disconnect()
            self.uri = original_uri  # 恢复原URI

    async def process_microphone_oneshot(self, duration=10):
        """使用OneShot接口处理麦克风输入"""
        logger.info(f"开始使用OneShot接口进行麦克风识别（录音{duration}秒）")
        
        # 将URI修改为oneshot端点
        original_uri = self.uri
        if "/sttRealtime" in self.uri:
            self.uri = self.uri.replace("/sttRealtime", "/oneshot")
        elif not "/oneshot" in self.uri:
            # 如果URI中没有具体端点，添加oneshot
            self.uri = self.uri.rstrip('/') + '/oneshot'
        
        if not await self.connect():
            self.uri = original_uri  # 恢复原URI
            return
            
        mic_stream = MicrophoneStream(self.sample_rate)
        
        try:
            mic_stream.start()
            
            # 创建接收结果的任务
            async def receive_results():
                while True:
                    result = await self.receive_result()
                    if result is None:
                        await asyncio.sleep(0.01)
                        continue
                    
                    result_type = result.get('type', 'unknown')
                    
                    if result_type == 'status':
                        status = result.get('status', '')
                        logger.info(f"📟 [状态] {status}")
                    elif result_type == 'result':
                        text = result.get('text', '')
                        lang = result.get('lang', 'auto')
                        emotion = result.get('emotion', '')
                        event = result.get('event', '')
                        timestamps = result.get('timestamps', [])
                        
                        logger.info(f"✅ [识别结果] 文本: {text}")
                        if lang != 'auto':
                            logger.info(f"    语言: {lang}")
                        if emotion:
                            logger.info(f"    情感: {emotion}")
                        if event:
                            logger.info(f"    事件: {event}")
                        if timestamps:
                            logger.info(f"    时间戳: {len(timestamps)}个")
                    elif result_type == 'error':
                        error_msg = result.get('message', '未知错误')
                        logger.error(f"❌ [错误] {error_msg}")
                    else:
                        logger.info(f"📨 [其他消息] {result}")
            
            # 启动接收任务
            receive_task = asyncio.create_task(receive_results())
            
            # 发送开始录音命令
            await self.send_control_message("start")
            await asyncio.sleep(0.5)  # 等待服务器准备
            
            # 录音和发送音频数据
            start_time = time.time()
            chunk_count = 0
            
            logger.info(f"开始录音 {duration} 秒...")
            
            try:
                while time.time() - start_time < duration:
                    # 读取音频块
                    audio_chunk = mic_stream.read()
                    await self.send_audio_chunk(audio_chunk)
                    
                    chunk_count += 1
                    elapsed = time.time() - start_time
                    if chunk_count % 20 == 0:  # 每2秒打印一次
                        remaining = duration - elapsed
                        logger.debug(f"录音中... 已录 {elapsed:.1f}s, 剩余 {remaining:.1f}s")
                    
                    await asyncio.sleep(0.01)  # 小延迟
                    
            except KeyboardInterrupt:
                logger.info("收到停止信号，提前结束录音")
            
            logger.info("录音结束，发送停止命令...")
            
            # 发送停止录音命令，开始处理
            await self.send_control_message("stop")
            
            # 等待处理结果
            logger.info("等待识别结果...")
            await asyncio.sleep(5.0)
            receive_task.cancel()
            
        except Exception as e:
            logger.error(f"OneShot麦克风处理错误: {e}")
        finally:
            mic_stream.stop()
            await self.disconnect()
            self.uri = original_uri  # 恢复原URI


async def main():
    parser = argparse.ArgumentParser(description="语音识别WebSocket客户端")
    parser.add_argument("--server", default="ws://localhost:8000/sttRealtime", 
                       help="WebSocket服务器地址 (默认: ws://localhost:8000/sttRealtime)")
    parser.add_argument("--sample-rate", type=int, default=16000,
                       help="音频采样率 (默认: 16000)")
    
    # 识别模式选择
    parser.add_argument("--mode", choices=["streaming", "oneshot"], default="streaming",
                       help="识别模式: streaming(流式) 或 oneshot(一句话) (默认: streaming)")
    
    # 音频源选择
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--file", help="音频文件路径")
    group.add_argument("--mic", action="store_true", help="使用麦克风")
    
    # 麦克风选项
    parser.add_argument("--duration", type=int, 
                       help="录音时长（秒）。流式模式：不指定则持续录音直到Ctrl+C；OneShot模式：默认10秒")
    
    args = parser.parse_args()
    
    client = ASRWebSocketClient(args.server, args.sample_rate)
    
    if args.file:
        # 文件模式
        if args.mode == "oneshot":
            await client.process_file_oneshot(args.file)
        else:
            await client.process_file(args.file)
    elif args.mic:
        # 麦克风模式
        if args.mode == "oneshot":
            duration = args.duration if args.duration else 10  # OneShot默认10秒
            await client.process_microphone_oneshot(duration)
        else:
            await client.process_microphone(args.duration)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("程序被用户中断")
    except Exception as e:
        logger.error(f"程序异常: {e}")
        import traceback
        traceback.print_exc()
