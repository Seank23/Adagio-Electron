document.querySelector('#analyzeBtn').onclick = async () => {
    const fileInput = document.querySelector('#audioFile');
    const arrayBuffer = await fileInput.files[0].arrayBuffer();

    const analysis = await window.api.analyzeAudio(arrayBuffer);

    document.querySelector('#result').innerText = JSON.stringify(analysis, null, 2);
}