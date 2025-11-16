document.querySelector('#loadBtn').onclick = async () => {
    const filePath = await window.api.selectAudioFile();
    if (!filePath) return;

    const result = await window.api.load(filePath);
    document.querySelector('#result').textContent = JSON.stringify(result, null, 2);
}

document.querySelector('#playBtn').onclick = async () => {
    const result = await window.api.play();
    document.querySelector('#result').textContent = JSON.stringify(result, null, 2);
}

document.querySelector('#pauseBtn').onclick = async () => {
    const result = await window.api.pause();
    document.querySelector('#result').textContent = JSON.stringify(result, null, 2);
}