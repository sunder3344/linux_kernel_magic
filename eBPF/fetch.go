package main

import (
	"fmt"
	"io"
	"net/http"
	"time"
)

//go:noinline
func FetchStock(client *http.Client, stockCode string) string {
	url := fmt.Sprintf("http://hq.sinajs.cn/list=%s", stockCode)
	
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		fmt.Printf("创建请求失败: %v\n", err)
		return ""
	}

	// 必须加上新浪要求的 Referer
	req.Header.Set("Referer", "https://finance.sina.com.cn")

	resp, err := client.Do(req)
	if err != nil {
		fmt.Printf("请求失败: %v\n", err)
		return ""
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return ""
	}
	return string(body[:50])
	//fmt.Printf("[Go App] 成功抓取数据: %s\n", string(body[:50])) // 只打印前50个字节
}

func main() {
	client := &http.Client{}
	fmt.Println("[Go App] 股票抓取程序启动，开始轮询...")
	
	for {
		result := FetchStock(client, "sh601006")
		fmt.Printf("[Go App] 成功抓取数据: %s\n", result) // 只打印前50个字节
		time.Sleep(5 * time.Second)
	}
}
